#ifndef ARIA2_H
#define ARIA2_H

#include "aria2_entity.h"

/*
https://aria2.github.io/manual/en/html/aria2c.html#rpc-interface
*/
// new aria2 instance
aria2_object *aria2_new(const char *uri, const char *token);
// launch aria2
int aria2_launch(aria2_object *aria2);
// aria2.shutdown
void aria2_shutdown(aria2_object *aria2);
// aria2.forceShutdown
void aria2_force_shutdown(aria2_object *aria2);
// aria2.addUri reurn gid
jsonrpc_response_result *aria2_add_uri(aria2_object *aria2, char **uris, int uris_len,
                                       key_value_pair options[], int options_len, int position);
// aria2.addTorrent reurn gid
jsonrpc_response_result *aria2_add_torrent(aria2_object *aria2, char *base64torrent,
                                           char **uris, int uris_len, key_value_pair options[], int options_len, int position);
// aria2.addMetalink reurn gid
jsonrpc_response_result *aria2_add_metalink(aria2_object *aria2, char *base64torrent,
                                            key_value_pair options[], int options_len, int position);
// aria2.getVersion return aria2_version
jsonrpc_response_result *aria2_get_version(aria2_object *aria2);
// aria2.remove return gid
jsonrpc_response_result *aria2_remove(aria2_object *aria2, char *gid);
// aria2.forceRemove  return gid
jsonrpc_response_result *aria2_force_remove(aria2_object *aria2, char *gid);
// aria2.pause  return gid
jsonrpc_response_result *aria2_pause(aria2_object *aria2, char *gid);
// aria2.pauseAll returns "OK" if it succeeds
jsonrpc_response_result *aria2_pause_all(aria2_object *aria2);
// aria2.forcePause  return gid
jsonrpc_response_result *aria2_force_pause(aria2_object *aria2, char *gid);
// aria2.forcePauseAll returns "OK" if it succeeds
jsonrpc_response_result *aria2_force_pause_all(aria2_object *aria2);
// aria2.unpause  return gid
jsonrpc_response_result *aria2_unpause(aria2_object *aria2, char *gid);
// aria2.unpauseAll returns "OK" if it succeeds
jsonrpc_response_result *aria2_unpause_all(aria2_object *aria2);
// aria2.tellStatus return aria2_item*
jsonrpc_response_result *aria2_tell_status(aria2_object *aria2, char *gid);
// aria2.getUris return aria2_item_file_uris*
jsonrpc_response_result *aria2_get_uris(aria2_object *aria2, char *gid);
// aria2.getFiles return aria2_item_files*
jsonrpc_response_result *aria2_get_files(aria2_object *aria2, char *gid);
// aria2.getPeers return aria2_peers*
jsonrpc_response_result *aria2_get_peers(aria2_object *aria2, char *gid);
// aria2.getServers reutrn aria2_servers*
jsonrpc_response_result *aria2_get_servers(aria2_object *aria2, char *gid);
// aria2.changePosition return int64
jsonrpc_response_result *aria2_change_position(aria2_object *aria2, char *gid, int pos, change_position_how how);
// aria2.tellActive return aria2_items*
jsonrpc_response_result *aria2_tell_active(aria2_object *aria2, int *out_size);
// aria2.tellWaiting return aria2_items*
jsonrpc_response_result *aria2_tell_waiting(aria2_object *aria2, int offset, int num, int *out_size);
// aria2.tellStopped return aria2_items*
jsonrpc_response_result *aria2_tell_stopped(aria2_object *aria2, int offset, int num, int *out_size);
/*
system.multicall[{ "aria2.tellActive" },{ "aria2.tellWaiting", 0, 1000 },{ "aria2.tellStopped", 0, 1000 }]
return aria2_item_all*
*/
jsonrpc_response_result *aria2_tell_all(aria2_object *aria2);
// aria2.getOption return aria2_options*
jsonrpc_response_result *aria2_get_option(aria2_object *aria2, char *gid);
// aria2.changeUri return aria2_int64_values*
jsonrpc_response_result *aria2_change_uri(aria2_object *aria2, char *gid, int fileindex,
                                          char **delUris, int delUris_len, char **addUris, int addUris_len, int position);
// aria2.changeOption returns "OK" if it succeeds
jsonrpc_response_result *aria2_change_option(aria2_object *aria2, char *gid, key_value_pair options[], int options_len);
// aria2.getGlobalOption  return aria2_options*
jsonrpc_response_result *aria2_get_global_option(aria2_object *aria2);
// aria2.changeGlobalOption returns "OK" if it succeeds
jsonrpc_response_result *aria2_change_global_option(aria2_object *aria2, key_value_pair options[], int options_len);
// aria2.purgeDownloadResult returns "OK" if it succeeds
jsonrpc_response_result *aria2_purge_download_result(aria2_object *aria2);
// aria2.removeDownloadResult returns "OK" if it succeeds
jsonrpc_response_result *aria2_remove_download_result(aria2_object *aria2, char *gid);
// aria2.getSessionInfo return sessionId(string)
jsonrpc_response_result *aria2_get_session_info(aria2_object *aria2);
// aria2.getGlobalStat return aria2_globalstat*
jsonrpc_response_result *aria2_get_global_stat(aria2_object *aria2);
// aria2.saveSession returns "OK" if it succeeds
jsonrpc_response_result *aria2_save_session(aria2_object *aria2);
#endif