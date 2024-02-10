/*
 * Copyright (C) 2024 Sébastien Helleu <flashcode@flashtux.org>
 *
 * This file is part of WeeChat, the extensible chat client.
 *
 * WeeChat is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * WeeChat is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with WeeChat.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef WEECHAT_PLUGIN_RELAY_REMOTE_H
#define WEECHAT_PLUGIN_RELAY_REMOTE_H

#include <gnutls/gnutls.h>

enum t_relay_remote_option
{
    RELAY_REMOTE_OPTION_URL = 0,       /* remote URL                        */
    RELAY_REMOTE_OPTION_PASSWORD,      /* password for remote relay         */
    RELAY_REMOTE_OPTION_TOTP_SECRET,   /* TOTP secret for remote relay      */
    /* number of relay remote options */
    RELAY_REMOTE_NUM_OPTIONS,
};

/* relay remote */

struct t_relay_remote
{
    char *name;                        /* internal remote name              */
    struct t_config_option *options[RELAY_REMOTE_NUM_OPTIONS];
    char *address;                     /* address                           */
    int port;                          /* port number                       */
    int tls;                           /* 1 if TLS is enabled               */
    enum t_relay_status status;        /* status (connecting, active,..)    */
    int sock;                          /* connected socket                  */
    gnutls_session_t gnutls_sess;      /* gnutls session (only if TLS used) */
    struct t_relay_remote *prev_remote;/* link to previous remote           */
    struct t_relay_remote *next_remote;/* link to next remote               */
};

extern char *relay_remote_option_string[];
extern char *relay_remote_option_default[];
extern struct t_relay_remote *relay_remotes;
extern struct t_relay_remote *last_relay_remote;
extern int relay_remotes_count;
extern struct t_relay_remote *relay_remotes_temp;
extern struct t_relay_remote *last_relay_remote_temp;

extern int relay_remote_search_option (const char *option_name);
extern int relay_remote_valid (struct t_relay_remote *remote);
extern struct t_relay_remote *relay_remote_search (const char *name);
extern struct t_relay_remote *relay_remote_search_by_number (int number);
extern int relay_remote_name_valid (const char *name);
extern int relay_remote_url_valid (const char *url);
extern struct t_relay_remote *relay_remote_alloc (const char *name);
extern void relay_remote_add (struct t_relay_remote *remote,
                              struct t_relay_remote **list_remotes,
                              struct t_relay_remote **last_list_remote);
extern struct t_relay_remote *relay_remote_new_with_options (const char *name,
                                                             struct t_config_option **options);
extern struct t_relay_remote *relay_remote_new (const char *name,
                                                const char *url,
                                                const char *password,
                                                const char *totp_secret);
extern struct t_relay_remote *relay_remote_new_with_infolist (struct t_infolist *infolist);
extern void relay_remote_set_status (struct t_relay_remote *remote,
                                     enum t_relay_status status);
extern int relay_remote_rename (struct t_relay_remote *remote, const char *name);
extern void relay_remote_free (struct t_relay_remote *remote);
extern void relay_remote_free_all ();
extern void relay_remote_disconnect (struct t_relay_remote *remote);
extern void relay_remote_disconnect_all ();
extern int relay_remote_add_to_infolist (struct t_infolist *infolist,
                                         struct t_relay_remote *remote,
                                         int force_disconnected_state);
extern void relay_remote_print_log ();

#endif /* WEECHAT_PLUGIN_RELAY_REMOTE_H */
