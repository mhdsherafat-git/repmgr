/*
 * repmgr-action-daemon.c
 *
 * Implements repmgrd actions for the repmgr command line utility
 * Copyright (c) 2ndQuadrant, 2010-2018
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "repmgr.h"

#include "repmgr-client-global.h"
#include "repmgr-action-daemon.h"

void
do_daemon_status(void)
{
	PGconn	   *conn = NULL;
	NodeInfoList nodes = T_NODE_INFO_LIST_INITIALIZER;
	NodeInfoListCell *cell = NULL;
	bool success;

	/* Connect to local database to obtain cluster connection data */
	log_verbose(LOG_INFO, _("connecting to database"));

	if (strlen(config_file_options.conninfo))
		conn = establish_db_connection(config_file_options.conninfo, true);
	else
		conn = establish_db_connection_by_params(&source_conninfo, true);

	success = get_all_node_records_with_upstream(conn, &nodes);

	if (success == false)
	{
		/* get_all_node_records_with_upstream() will print error message */
		PQfinish(conn);
		exit(ERR_BAD_CONFIG);
	}

	if (nodes.node_count == 0)
	{
		log_error(_("no node records were found"));
		log_hint(_("ensure at least one node is registered"));
		PQfinish(conn);
		exit(ERR_BAD_CONFIG);
	}

	for (cell = nodes.head; cell; cell = cell->next)
	{
		bool is_running = false;
		int pid = UNKNOWN_PID;

		cell->node_info->conn = establish_db_connection_quiet(cell->node_info->conninfo);

		if (PQstatus(cell->node_info->conn) != CONNECTION_OK)
		{
			printf("unable to connect to node %i\n", cell->node_info->node_id);
		}
		else
		{
			is_running = repmgrd_is_running(cell->node_info->conn);
			pid = repmgrd_get_pid(cell->node_info->conn);

			printf("node %i: %s %i\n",
				   cell->node_info->node_id,
				   is_running ? "running" : "not running",
				   pid);
		}
		PQfinish(cell->node_info->conn);
	}
}



void do_daemon_help(void)
{
	print_help_header();

	printf(_("Usage:\n"));
	printf(_("    %s [OPTIONS] daemon status\n"), progname());

	puts("");
}
