#include "Server.hh"

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <phosg/Encoding.hh>
#include <phosg/Network.hh>
#include <phosg/Strings.hh>
#include <phosg/Time.hh>

#include "Loggers.hh"
#include "PSOProtocol.hh"
#include "ReceiveCommands.hh"

using namespace std;
using namespace std::placeholders;



void Server::disconnect_client(shared_ptr<Client> c) {
  if (c->channel.is_virtual_connection) {
    server_log.info("Client disconnected: C-%" PRIX64 " on virtual connection %p",
        c->id, c->channel.bev.get());
  } else {
    server_log.info("Client disconnected: C-%" PRIX64 " on fd %d",
        c->id, bufferevent_getfd(c->channel.bev.get()));
  }

  this->channel_to_client.erase(&c->channel);
  c->channel.disconnect();

  try {
    process_disconnect(this->state, c);
  } catch (const exception& e) {
    server_log.warning("Error during client disconnect cleanup: %s", e.what());
  }
  // c is destroyed here (process_disconnect should remove any other references
  // to it, e.g. from Lobby objects)
}

void Server::dispatch_on_listen_accept(
    struct evconnlistener* listener, evutil_socket_t fd,
    struct sockaddr* address, int socklen, void* ctx) {
  reinterpret_cast<Server*>(ctx)->on_listen_accept(listener, fd, address,
      socklen);
}

void Server::dispatch_on_listen_error(struct evconnlistener* listener,
    void* ctx) {
  reinterpret_cast<Server*>(ctx)->on_listen_error(listener);
}

void Server::on_listen_accept(struct evconnlistener* listener,
    evutil_socket_t fd, struct sockaddr*, int) {

  int listen_fd = evconnlistener_get_fd(listener);
  ListeningSocket* listening_socket;
  try {
    listening_socket = &this->listening_sockets.at(listen_fd);
  } catch (const out_of_range& e) {
    server_log.warning("Can\'t determine version for socket %d; disconnecting client",
        listen_fd);
    close(fd);
    return;
  }

  struct bufferevent *bev = bufferevent_socket_new(this->base.get(), fd,
      BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);
  shared_ptr<Client> c(new Client(
      bev, listening_socket->version, listening_socket->behavior));
  c->channel.on_command_received = Server::on_client_input;
  c->channel.on_error = Server::on_client_error;
  c->channel.context_obj = this;
  this->channel_to_client.emplace(&c->channel, c);

  server_log.info("Client connected: C-%" PRIX64 " on fd %d via %d (%s)",
      c->id, fd, listen_fd, listening_socket->addr_str.c_str());

  try {
    process_connect(this->state, c);
  } catch (const exception& e) {
    server_log.warning("Error during client initialization: %s", e.what());
    this->disconnect_client(c);
  }
}

void Server::connect_client(
    struct bufferevent* bev, uint32_t address, uint16_t client_port,
    uint16_t server_port, GameVersion version, ServerBehavior initial_state) {
  shared_ptr<Client> c(new Client(bev, version, initial_state));
  c->channel.on_command_received = Server::on_client_input;
  c->channel.on_error = Server::on_client_error;
  c->channel.context_obj = this;

  server_log.info("Client connected: C-%" PRIX64 " on virtual connection %p via T-%hu-%s-%s-VI",
      c->id,
      bev,
      server_port,
      name_for_version(version),
      name_for_server_behavior(initial_state));

  this->channel_to_client.emplace(&c->channel, c);

  // Manually set the remote address, since the bufferevent has no fd and the
  // Channel constructor can't figure out the virtual remote address
  auto* remote_sin = reinterpret_cast<sockaddr_in*>(&c->channel.remote_addr);
  remote_sin->sin_family = AF_INET;
  remote_sin->sin_addr.s_addr = htonl(address);
  remote_sin->sin_port = htons(client_port);

  try {
    process_connect(this->state, c);
  } catch (const exception& e) {
    server_log.error("Error during client initialization: %s", e.what());
    this->disconnect_client(c);
  }
}

void Server::on_listen_error(struct evconnlistener* listener) {
  int err = EVUTIL_SOCKET_ERROR();
  server_log.error("Failure on listening socket %d: %d (%s)",
      evconnlistener_get_fd(listener), err, evutil_socket_error_to_string(err));
  event_base_loopexit(this->base.get(), nullptr);
}

void Server::on_client_input(Channel& ch, uint16_t command, uint32_t flag, std::string& data) {
  Server* server = reinterpret_cast<Server*>(ch.context_obj);
  shared_ptr<Client> c = server->channel_to_client.at(&ch);

  if (c->should_disconnect) {
    server->disconnect_client(c);
  } else {
    try {
      process_command(server->state, c, command, flag, data);
    } catch (const exception& e) {
      server_log.warning("Error processing client command: %s", e.what());
      c->should_disconnect = true;
    }
    if (c->should_disconnect) {
      server->disconnect_client(c);
    }
  }
}

void Server::on_client_error(Channel& ch, short events) {
  Server* server = reinterpret_cast<Server*>(ch.context_obj);
  shared_ptr<Client> c = server->channel_to_client.at(&ch);

  if (events & BEV_EVENT_ERROR) {
    int err = EVUTIL_SOCKET_ERROR();
    server_log.warning("Client caused error %d (%s)", err,
        evutil_socket_error_to_string(err));
  }
  if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
    server->disconnect_client(c);
  }
}

Server::Server(
    shared_ptr<struct event_base> base,
    shared_ptr<ServerState> state)
  : base(base), state(state) { }

void Server::listen(
    const std::string& addr_str,
    const string& socket_path,
    GameVersion version,
    ServerBehavior behavior) {
  int fd = ::listen(socket_path, 0, SOMAXCONN);
  server_log.info("Listening on Unix socket %s on fd %d as %s",
      socket_path.c_str(), fd, addr_str.c_str());
  this->add_socket(addr_str, fd, version, behavior);
}

void Server::listen(
    const std::string& addr_str,
    const string& addr,
    int port,
    GameVersion version,
    ServerBehavior behavior) {
  int fd = ::listen(addr, port, SOMAXCONN);
  string netloc_str = render_netloc(addr, port);
  server_log.info("Listening on TCP interface %s on fd %d as %s",
      netloc_str.c_str(), fd, addr_str.c_str());
  this->add_socket(addr_str, fd, version, behavior);
}

void Server::listen(const std::string& addr_str, int port, GameVersion version, ServerBehavior behavior) {
  this->listen(addr_str, "", port, version, behavior);
}

Server::ListeningSocket::ListeningSocket(Server* s, const std::string& addr_str,
    int fd, GameVersion version, ServerBehavior behavior) :
    addr_str(addr_str), fd(fd), version(version), behavior(behavior), listener(
        evconnlistener_new(s->base.get(), Server::dispatch_on_listen_accept, s,
          LEV_OPT_REUSEABLE, 0, this->fd), evconnlistener_free) {
  evconnlistener_set_error_cb(this->listener.get(),
      Server::dispatch_on_listen_error);
}

void Server::add_socket(
    const std::string& addr_str,
    int fd,
    GameVersion version,
    ServerBehavior behavior) {
  this->listening_sockets.emplace(piecewise_construct, forward_as_tuple(fd),
      forward_as_tuple(this, addr_str, fd, version, behavior));
}

shared_ptr<Client> Server::get_client() const {
  if (this->channel_to_client.empty()) {
    throw runtime_error("no clients on game server");
  }
  if (this->channel_to_client.size() > 1) {
    throw runtime_error("multiple clients on game server");
  }
  return this->channel_to_client.begin()->second;
}
