#pragma once

#include <atomic>
#include <map>
#include <unordered_map>
#include <string>
#include <memory>
#include <vector>
#include <set>

#include "Client.hh"
#include "Items.hh"
#include "LevelTable.hh"
#include "License.hh"
#include "Lobby.hh"
#include "Menu.hh"
#include "Quest.hh"



// Forwawrd declaration due to reference cycle
class ProxyServer;

struct PortConfiguration {
  std::string name;
  uint16_t port;
  GameVersion version;
  ServerBehavior behavior;
};

struct ServerState {
  enum class RunShellBehavior {
    DEFAULT = 0,
    ALWAYS,
    NEVER,
  };

  std::u16string name;
  std::unordered_map<std::string, std::shared_ptr<PortConfiguration>> name_to_port_config;
  std::unordered_map<uint16_t, std::shared_ptr<PortConfiguration>> number_to_port_config;
  std::string username;
  uint16_t dns_server_port;
  std::vector<std::string> ip_stack_addresses;
  bool ip_stack_debug;
  bool allow_unregistered_users;
  RunShellBehavior run_shell_behavior;
  PSOBBEncryption::KeyFile default_key_file;
  std::shared_ptr<const QuestIndex> quest_index;
  std::shared_ptr<const LevelTable> level_table;
  std::shared_ptr<const BattleParamTable> battle_params;
  std::shared_ptr<const CommonItemCreator> common_item_creator;

  std::shared_ptr<LicenseManager> license_manager;

  std::vector<MenuItem> main_menu;
  std::shared_ptr<std::vector<MenuItem>> information_menu_pc;
  std::shared_ptr<std::vector<MenuItem>> information_menu_gc;
  std::shared_ptr<std::vector<std::u16string>> information_contents;
  std::vector<MenuItem> proxy_destinations_menu_pc;
  std::vector<MenuItem> proxy_destinations_menu_gc;
  std::vector<std::pair<std::string, uint16_t>> proxy_destinations_pc;
  std::vector<std::pair<std::string, uint16_t>> proxy_destinations_gc;
  std::pair<std::string, uint16_t> proxy_destination_patch;
  std::u16string welcome_message;

  std::map<int64_t, std::shared_ptr<Lobby>> id_to_lobby;
  std::vector<std::shared_ptr<Lobby>> public_lobby_search_order;
  std::vector<std::shared_ptr<Lobby>> public_lobby_search_order_ep3;
  std::atomic<int32_t> next_lobby_id;
  uint8_t pre_lobby_event;
  int32_t ep3_menu_song;

  std::map<std::string, uint32_t> all_addresses;
  uint32_t local_address;
  uint32_t external_address;

  // TODO: This is only here because the menu selection handler has to call
  // delete_session on it. Find a cleaner way to do this.
  std::shared_ptr<ProxyServer> proxy_server;

  ServerState();

  void add_client_to_available_lobby(std::shared_ptr<Client> c);
  void remove_client_from_lobby(std::shared_ptr<Client> c);
  void change_client_lobby(std::shared_ptr<Client> c,
      std::shared_ptr<Lobby> new_lobby);

  void send_lobby_join_notifications(std::shared_ptr<Lobby> l,
      std::shared_ptr<Client> joining_client);

  std::shared_ptr<Lobby> find_lobby(uint32_t lobby_id);
  std::vector<std::shared_ptr<Lobby>> all_lobbies();

  void add_lobby(std::shared_ptr<Lobby> l);
  void remove_lobby(uint32_t lobby_id);

  std::shared_ptr<Client> find_client(const char16_t* identifier = nullptr,
    uint64_t serial_number = 0, std::shared_ptr<Lobby> l = nullptr);

  uint32_t connect_address_for_client(std::shared_ptr<Client> c);

  std::shared_ptr<const std::vector<MenuItem>> information_menu_for_version(GameVersion version);
  const std::vector<MenuItem>& proxy_destinations_menu_for_version(GameVersion version);
  const std::vector<std::pair<std::string, uint16_t>>& proxy_destinations_for_version(GameVersion version);

  void set_port_configuration(
    const std::vector<PortConfiguration>& port_configs);
};
