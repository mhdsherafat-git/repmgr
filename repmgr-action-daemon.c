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




// repmgrd start time?
// repmgrd mode
// priority

typedef enum
{
	STATUS_ID = 0,
	STATUS_NAME,
	STATUS_ROLE,
	STATUS_PID,
	STATUS_RUNNING,
	STATUS_PAUSED
}			StatusHeader;

#define STATUS_HEADER_COUNT 6

struct ColHeader headers_status[STATUS_HEADER_COUNT];

typedef struct RepmgrdInfo {
	int pid;
	char pid_text[MAXLEN];
	char pid_file[MAXLEN];
	bool running;
	bool paused;
} RepmgrdInfo;

void
do_daemon_status(void)
{
	PGconn	   *conn = NULL;
	NodeInfoList nodes = T_NODE_INFO_LIST_INITIALIZER;
	NodeInfoListCell *cell = NULL;
	bool success;
	int i;
	RepmgrdInfo **repmgrd_info;

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

	repmgrd_info = (RepmgrdInfo **) pg_malloc0(sizeof(RepmgrdInfo *) * nodes.node_count);

	strncpy(headers_status[STATUS_ID].title, _("ID"), MAXLEN);
	strncpy(headers_status[STATUS_NAME].title, _("Name"), MAXLEN);
	strncpy(headers_status[STATUS_ROLE].title, _("Role"), MAXLEN);
	strncpy(headers_status[STATUS_PID].title, _("PID"), MAXLEN);
	strncpy(headers_status[STATUS_RUNNING].title, _("Running?"), MAXLEN);
	strncpy(headers_status[STATUS_PAUSED].title, _("Paused?"), MAXLEN);

	for (i = 0; i < STATUS_HEADER_COUNT; i++)
	{
		headers_status[i].max_length = strlen(headers_status[i].title);
	}

	i = 0;

	for (cell = nodes.head; cell; cell = cell->next)
	{
		int j;

		repmgrd_info[i] = pg_malloc0(sizeof(RepmgrdInfo));
		repmgrd_info[i]->pid = UNKNOWN_PID;
		repmgrd_info[i]->running = false;
		repmgrd_info[i]->paused = false;

		cell->node_info->conn = establish_db_connection_quiet(cell->node_info->conninfo);


		if (PQstatus(cell->node_info->conn) != CONNECTION_OK)
		{
			printf("unable to connect to node %i\n", cell->node_info->node_id);
		}
		else
		{
			repmgrd_info[i]->pid = repmgrd_get_pid(cell->node_info->conn);

			if (repmgrd_info[i]->pid == UNKNOWN_PID)
			{
				maxlen_snprintf(repmgrd_info[i]->pid_text, "%s", _("n/a"));
			}
			else
			{
				maxlen_snprintf(repmgrd_info[i]->pid_text, "%i", repmgrd_info[i]->pid);
			}

			repmgrd_info[i]->running = repmgrd_is_running(cell->node_info->conn);
			repmgrd_info[i]->paused = repmgrd_is_paused(cell->node_info->conn);
		}
		PQfinish(cell->node_info->conn);

		headers_status[STATUS_NAME].cur_length = strlen(cell->node_info->node_name);
		headers_status[STATUS_ROLE].cur_length = strlen(get_node_type_string(cell->node_info->type));
		headers_status[STATUS_PID].cur_length = strlen(repmgrd_info[i]->pid_text);

		for (j = 0; j < STATUS_HEADER_COUNT; j++)
		{
			if (headers_status[j].cur_length > headers_status[j].max_length)
			{
				headers_status[j].max_length = headers_status[j].cur_length;
			}
		}

		i++;
	}

	/* Print column header row (text mode only) */
	if (runtime_options.output_mode == OM_TEXT)
	{
		print_status_header(STATUS_HEADER_COUNT, headers_status);
	}

	i = 0;

	for (cell = nodes.head; cell; cell = cell->next)
	{
		if (runtime_options.output_mode == OM_CSV)
		{
			// XXX implement
		}
		else
		{
			printf(" %-*i ",  headers_status[STATUS_ID].max_length, cell->node_info->node_id);
			printf("| %-*s ", headers_status[STATUS_NAME].max_length, cell->node_info->node_name);
			printf("| %-*s ", headers_status[STATUS_ROLE].max_length, get_node_type_string(cell->node_info->type));
			printf("| %-*s ", headers_status[STATUS_PID].max_length, repmgrd_info[i]->pid_text);
			printf("| %-*s ", headers_status[STATUS_RUNNING].max_length, repmgrd_info[i]->running ? "yes" : "no");
			printf("| %-*s ", headers_status[STATUS_PAUSED].max_length, repmgrd_info[i]->paused ? "yes" : "no");
		}
		printf("\n");

		i++;
	}

	free(repmgrd_info);
}



void do_daemon_help(void)
{
	print_help_header();

	printf(_("Usage:\n"));
	printf(_("    %s [OPTIONS] daemon status\n"), progname());

	puts("");
}
