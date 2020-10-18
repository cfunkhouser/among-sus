#define _POSIX_C_SOURCE 200112L
#define _XOPEN_SOURCE 700
#include <arpa/inet.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define NUM_PLAYERS 10
#define NUM_SHORT 6
#define NUM_LONG 2
#define NUM_CHATS 50
#define MIN_NAME 2
#define MAX_NAME 10

enum game_stage {
	STAGE_LOBBY,
	STAGE_PLAYING,
	STAGE_DISCUSS,
};

enum player_stage {
	PLAYER_STAGE_NAME,
	PLAYER_STAGE_LOBBY,
	PLAYER_STAGE_MAIN,
	PLAYER_STAGE_DISCUSS,
};

enum player_task_short {
	TASK_CAFE_TRASH,
	TASK_CAFE_COFFEE,
	TASK_CAFE_WIRES,
	TASK_STORAGE_TRASH,
	TASK_ELECTRICAL_WIRES,
	TASK_ELECTRICAL_BREAKERS,
	TASK_ADMIN_WIRES,
	TASK_NAVIGATION_WIRES,
	TASK_WEAPONS_WIRES,
	TASK_SHIELDS_WIRES,
	TASK_O2_WIRES,
	TASK_O2_CLEAN,
	TASK_MEDBAY_WIRES,
	TASK_UPPER_CATALYZER,
	TASK_LOWER_CATALYZER,
	TASK_UPPER_COMPRESSION_COIL,
	TASK_LOWER_COMPRESSION_COIL,
	TASK_SHORT_COUNT,
};

const char short_task_descriptions[][45] = {
	"Empty the cafeteria trash",
	"Start the coffee maker in the cafeteria",
	"Fix cafeteria wireing",
	"Empty the storage trash chute",
	"Fix wiring in electrical",
	"Reset breakers in electrical",
	"Fix wiring in admin",
	"Fix wiring in navigation",
	"Fix wiring in weapons",
	"Fix wiring in shields",
	"Fix wiring in o2",
	"Clean oxygenator output in o2",
	"Fix wiring in medbay",
	"Check catalyzer in upper engine",
	"Check catalyzer in lower engine",
	"Replace compression coil in upper engine",
	"Replace compression coil in lower engine",
};

enum player_task_long {
	TASK_STORAGE_COUNT,
	TASK_O2_LOG,
	TASK_REACTOR_LOG,
	TASK_ADMIN_PIN,
	TASK_LONG_COUNT
};

const char long_task_descriptions[][45] = {
	"Take count of the boxes in storage",
	"Log o2 numbers",
	"Log reactor numbers",
	"Enter pin at admin",
};

enum player_location {
	LOC_CAFETERIA,
	LOC_REACTOR,
	LOC_UPPER_ENGINE,
	LOC_LOWER_ENGINE,
	LOC_SECURITY,
	LOC_MEDBAY,
	LOC_ELECTRICAL,
	LOC_STORAGE,
	LOC_ADMIN,
	LOC_COMMUNICATIONS,
	LOC_O2,
	LOC_WEAPONS,
	LOC_SHIELDS,
	LOC_NAVIGATION,
	LOC_COUNT,
};

const char locations[][45] = {
	[LOC_CAFETERIA] = "cafeteria",
	[LOC_REACTOR] = "reactor",
	[LOC_UPPER_ENGINE] = "upper",
	[LOC_LOWER_ENGINE] = "lower",
	[LOC_SECURITY] = "security",
	[LOC_MEDBAY] = "medbay",
	[LOC_ELECTRICAL] = "electrical",
	[LOC_STORAGE] = "storage",
	[LOC_ADMIN] = "admin",
	[LOC_COMMUNICATIONS] = "communications",
	[LOC_O2] = "o2",
	[LOC_WEAPONS] = "weapons",
	[LOC_SHIELDS] = "shields",
	[LOC_NAVIGATION] = "navigation",
};

enum player_location doors[][10] = {
	[LOC_CAFETERIA] = { LOC_MEDBAY, LOC_ADMIN, LOC_WEAPONS, -1 },
	[LOC_REACTOR] = { LOC_UPPER_ENGINE, LOC_SECURITY, LOC_LOWER_ENGINE, -1 },
	[LOC_UPPER_ENGINE] = { LOC_REACTOR, LOC_ELECTRICAL, -1 },
	[LOC_LOWER_ENGINE] = { LOC_REACTOR, LOC_ELECTRICAL, -1 },
	[LOC_SECURITY] = { LOC_UPPER_ENGINE, LOC_REACTOR, LOC_LOWER_ENGINE, -1 },
	[LOC_MEDBAY] = { LOC_UPPER_ENGINE, LOC_CAFETERIA, -1 },
	[LOC_ELECTRICAL] = { LOC_LOWER_ENGINE, LOC_STORAGE, -1 },
	[LOC_STORAGE] = { LOC_ELECTRICAL, LOC_ADMIN, LOC_COMMUNICATIONS, LOC_SHIELDS, -1 },
	[LOC_ADMIN] = { LOC_CAFETERIA, LOC_STORAGE, -1 },
	[LOC_COMMUNICATIONS] = { LOC_STORAGE, LOC_SHIELDS, -1 },
	[LOC_O2] = { LOC_SHIELDS, LOC_WEAPONS, LOC_NAVIGATION, -1 },
	[LOC_WEAPONS] = { LOC_CAFETERIA, LOC_O2, LOC_NAVIGATION, -1 },
	[LOC_SHIELDS] = { LOC_STORAGE, LOC_COMMUNICATIONS, LOC_O2, LOC_NAVIGATION, -1 },
	[LOC_NAVIGATION] = { LOC_WEAPONS, LOC_O2, LOC_SHIELDS, -1 },
};

const char descriptions[][256] = {
	[LOC_CAFETERIA] = "You are standing in the middle of the cafeteria, in the center there's an emergency button\n",
	[LOC_REACTOR] = "You are in the reactor room, it seems to be running normally\n",
	[LOC_UPPER_ENGINE] = "You are in a small room, mostly filled up by an engine.\n",
	[LOC_LOWER_ENGINE] = "You are in a small room, mostly filled up by an engine.\n",
	[LOC_SECURITY] = "You are in a small room filled with monitors, the monitors are displaying camera images showing an overview of the ship\n",
	[LOC_MEDBAY] = "You are in a room with beds and a medical scanner.\n",
	[LOC_ELECTRICAL] = "You are in a room filled with equipment racks. Some of them have wires sticking out of them\n",
	[LOC_STORAGE] = "You are in a large room filled with boxes. One of the walls has a large door to the outside\n",
	[LOC_ADMIN] = "You are in a nice carpeted room with a holographic map in the middle\n",
	[LOC_COMMUNICATIONS] = "You are in a small room with what looks like radio equipment\n",
	[LOC_O2] = "You are in a room with plants in terrariums and life support equipment\n",
	[LOC_WEAPONS] = "You are in a circular room with a targeting system in the middle and a view of outer space\n",
	[LOC_SHIELDS] = "You are in a circular room with glowing tubes and a control panel for the shields\n",
	[LOC_NAVIGATION] = "You are all the way in the front of the ship in a room with the ship controls and a great view of space\n",
};

struct player {
	int fd;
	enum player_stage stage;
	char name[MAX_NAME + 1];
	int is_admin;
	int is_imposter;
	enum player_location location;
	int in_vent;
	int is_alive;
	int is_found;
	int has_cooldown;
	int voted;
	int votes;

	enum player_task_short short_tasks[NUM_SHORT];
	int short_tasks_done[NUM_SHORT];
	enum player_task_long long_tasks[NUM_LONG];
	int long_tasks_done[NUM_LONG];
};

struct gamestate {
	enum game_stage stage;
	int has_admin;
	int players;
	int is_reactor_meltdown;
	int chats_left;
	int skips;
};

struct gamestate state;
struct player players[NUM_PLAYERS];
fd_set rfds, afds;

void
broadcast(char* message, int notfd)
{
	char buf[1024];
	int pid;

	printf("*> %s\n", message);

	snprintf(buf, sizeof(buf), "%s\n", message);

	for (pid = 0; pid < NUM_PLAYERS; pid++) {
		if (players[pid].fd == -1 || players[pid].fd == notfd
				|| players[pid].stage == PLAYER_STAGE_NAME)
			continue;

		write(players[pid].fd, buf, strlen(buf));
	}
}

void
player_move(int pid, enum player_location location)
{
	char buf[100];
	printf("Moving player %d to %d\n", pid, location);
	players[pid].location = location;
	players[pid].has_cooldown = 0;

	// body detection
	for(int i=0; i<NUM_PLAYERS;i++) {
		if (players[i].location != players[pid].location || i == pid
				|| players[i].fd == -1
				|| players[i].is_alive == 1
				|| players[i].is_found == 1)
			continue;

		snprintf(buf, sizeof(buf), "you enter the room and see the body of [%s] laying on the floor\n", players[i].name);
		write(players[pid].fd, buf, strlen(buf));
	}
}

void
end_game()
{
	broadcast("------------------------", -1);
	broadcast("The game has ended, returning to lobby", -1);
	state.stage = STAGE_LOBBY;

	for(int i=0; i<NUM_PLAYERS;i++) {
		if (players[i].fd == -1)
			continue;
		players[i].stage = PLAYER_STAGE_LOBBY;
	}
}

// TODO: check if crew has won by completing tasks
void
check_win_condition(void)
{
	char buf[100];
	int alive = 0;
	int iid;

	for(int i=0; i<NUM_PLAYERS;i++) {
		if (players[i].fd != -1 && players[i].is_imposter)
			iid = i;

		if (players[i].is_imposter == 1 && players[i].is_alive == 0) {
			broadcast("The crew won", -1);
			end_game();
			return;
		}

		if (players[i].fd != -1 &&
				players[i].is_alive == 1 &&
				players[i].is_imposter == 0)
			alive++;
	}

	if (alive == 1) {
		broadcast("The imposter is alone with the last crewmate and murders him", -1);
		snprintf(buf, strlen(buf), "The imposter was [%s] all along...", players[iid].name);
		broadcast(buf, -1);
		end_game();
	}
}

void
task_completed(int pid, int task_id, int long_task)
{
	// Mark task completed for player
	if (!long_task) {
		for(int i=0; i<NUM_SHORT; i++) {
			if (players[pid].short_tasks[i] == task_id) {
				players[pid].short_tasks_done[i] = 1;
			}
		}
	} else {
		for(int i=0; i<NUM_LONG; i++) {
			if (players[pid].long_tasks[i] == task_id) {
				players[pid].long_tasks_done[i] = 1;
			}
		}
	}

	check_win_condition();
}

void
player_list_tasks(int pid)
{
	char buf[100];

	for(int i=0;i<TASK_SHORT_COUNT;i++){
		for(int j=0;j<NUM_SHORT;j++) {
			if(players[pid].short_tasks[j] == i) {
				const char *cm;
				if(players[pid].short_tasks_done[j]) {
					cm = "*";
				} else {
					cm = " ";
				}
				snprintf(buf, sizeof(buf), " [%s] %s\n", cm,
					short_task_descriptions[i]);
				write(players[pid].fd, buf, strlen(buf));
			}
		}
	}
	for(int i=0;i<TASK_LONG_COUNT;i++){
		for(int j=0;j<NUM_LONG;j++) {
			if(players[pid].long_tasks[j] == i) {
				const char *cm;
				if(players[pid].long_tasks_done[j]) {
					cm = "*";
				} else {
					cm = " ";
				}
				snprintf(buf, sizeof(buf), " [%s] %s\n", cm,
					long_task_descriptions[i]);
				write(players[pid].fd, buf, strlen(buf));
			}
		}
	}
	snprintf(buf, sizeof(buf), "# ");
	write(players[pid].fd, buf, strlen(buf));
}

void
player_kill(int pid, int tid)
{
	char buf[100];

	if(players[pid].location != players[tid].location
			|| players[tid].is_imposter)
		return;

	// so sad
	players[tid].is_alive = 0;

	// less murdering, reset by movement
	players[pid].has_cooldown = 1;
	
	// notify player of their recent death
	snprintf(buf, sizeof(buf), "It turns out %s is the imposter, sadly the way you know is that you died.\n",
		players[pid].name);
	write(players[tid].fd, buf, strlen(buf));

	// notify bystanders
	for(int i=0; i<NUM_PLAYERS;i++) {
		if (i == pid || players[i].fd == -1 || players[i].is_alive == 0
				|| players[i].location != players[pid].location)
			continue;

		snprintf(buf, sizeof(buf), "someone killed [%s] while you were in the room\n",
			players[tid].name);
		write(players[i].fd, buf, strlen(buf));
	}
	
	check_win_condition();
}

void
start_discussion(int pid, int bid)
{
	char buf[100];

	state.stage = STAGE_DISCUSS;
	state.skips = 0;

	// switch everyone to the discussion state and mark bodies found
	for(int i=0; i<NUM_PLAYERS;i++) {
		if (players[i].fd == -1)
			continue;

		players[i].stage = PLAYER_STAGE_DISCUSS;
		players[i].voted = 0;
		players[i].votes = 0;

		if (!players[i].is_alive)
			players[i].is_found = 1;
	}
	broadcast("------------------------", -1);
	// Inform everyone
	if(bid == -1) {
		// Emergency button was pressed
		snprintf(buf, sizeof(buf), "\nAn emergency meeting was called by [%s]", players[pid].name);
	} else {
		// Body was reported
		snprintf(buf, sizeof(buf), "\nThe body of [%s] was found by [%s]", players[bid].name, players[pid].name);
	}
	broadcast(buf, -1);

	// List the state of the players
	broadcast("Players:", -1);
	for(int i=0; i<NUM_PLAYERS;i++) {
		if (players[i].fd == -1)
			continue;
		if (players[i].is_alive) {
			snprintf(buf, sizeof(buf), "* %d [%s]", i, players[i].name);
		} else {
			snprintf(buf, sizeof(buf), "* %d [%s] (dead)", i, players[i].name);
		}
		broadcast(buf, -1);
	}

	// Inform people of the chat limit
	snprintf(buf, sizeof(buf), "Discuss, there are %d messages left", NUM_CHATS);
	state.chats_left = NUM_CHATS;
	broadcast(buf, -1);
}

void
back_to_playing()
{
	state.stage = STAGE_PLAYING;
	// switch everyone to the playing state
	for(int i=0; i<NUM_PLAYERS;i++) {
		if (players[i].fd == -1)
			continue;

		players[i].stage = PLAYER_STAGE_MAIN;
	}
	broadcast("-- Voting has ended, back to the ship --\n\n# ", -1);
}

void
discussion(int pid, char* input)
{
	char buf[300];
	int vote = 0, max_votes = 0, tie = 0, winner = -1, crew_alive = 0;
	char temp[5];

	// TODO: implement broadcast to dead players
	if (players[pid].is_alive == 0)
		return;

	if (input[0] == '/' && input[1] != '/') {
		if (strncmp(input, "/vote ", 6) == 0 || strncmp(input, "/skip", 6) == 0) {
			if (players[pid].voted) {
				snprintf(buf, sizeof(buf), "You can only vote once\n");
				write(players[pid].fd, buf, strlen(buf));
				return;
			}
			if (input[1] == 's') {
				vote = -1;
			} else {
				char *endptr = NULL;
				strncpy(temp, &input[6], 4);
				printf("Decoding '%s' now\n", temp);
				vote = strtol(temp, &endptr, 10);
				if (!endptr || endptr[0] != '\0') {
					snprintf(buf, sizeof(buf), "Invalid vote, not an integer\n");
					write(players[pid].fd, buf, strlen(buf));
					return;
				}
			}

			if (vote == -1) {
				printf("[%s] voted to skip\n", players[pid].name);
			} else {
				printf("[%s] voted for %d\n", players[pid].name, vote);
			}
			if(vote < -1 || vote > NUM_PLAYERS-1 || players[vote].fd == -1) {
				snprintf(buf, sizeof(buf), "Invalid vote, no such player\n");
				write(players[pid].fd, buf, strlen(buf));
				return;
			}
			players[pid].voted = 1;
			if (vote == -1) {
				state.skips++;
			} else {
				players[vote].votes++;
			}
check_votes:
			// Check if voting is complete
			for(int i=0;i<NUM_PLAYERS;i++) {
				if(players[i].fd != -1 && 
						players[i].voted == 0 &&
						players[i].is_alive == 1) {
					printf("No vote from [%s] yet\n", players[i].name);
					goto not_yet;
				}
			}

			printf("Voting complete\n");

			// Count votes
			max_votes = state.skips;
			for(int i=0;i<NUM_PLAYERS;i++) {
				if(players[i].fd == -1)
					continue;

				if(players[i].votes > max_votes){
					max_votes = players[i].votes;
					tie = 0;
					winner = i;
					continue;
				}

				if(players[i].votes == max_votes) {
					tie = 1;
				}
			}

			printf("Vote winner: %d\n", winner);

			if (tie) {
				broadcast("The voting ended in a tie", -1);
				back_to_playing();
				return;
			} else if (winner == -1) {
				snprintf(buf, sizeof(buf), "The crew voted to skip\n");
				broadcast(buf, -1);
				back_to_playing();
				return;
			} else {
				snprintf(buf, sizeof(buf), "The crew voted to eject [%s]\n", players[winner].name);
				broadcast(buf, -1);
			}

			// dramatic pause
			for(int i=0;i<5;i++) {
				sleep(1);
				broadcast(".", -1);
			}

			players[winner].is_alive = 0;
			if (players[winner].is_imposter) {
				snprintf(buf, sizeof(buf), "It turns out [%s] was an imposter", players[winner].name);
				broadcast(buf, -1);
			} else {
				snprintf(buf, sizeof(buf), "Sadly, [%s] was not an imposter", players[winner].name);
				broadcast(buf, -1);

			}
			check_win_condition();
			return;

not_yet:
			broadcast("A vote has been cast", -1);
		} else if (strncmp(input, "/help", 6) == 0) {
			snprintf(buf, sizeof(buf), "Commands: /vote [id], /skip, /list\n");
			write(players[pid].fd, buf, strlen(buf));
		} else if (strncmp(input, "/list", 6) == 0) {
			for(int i=0; i<NUM_PLAYERS;i++) {
				if (players[i].fd == -1)
					continue;
				if (players[i].is_alive && players[i].voted) {
					snprintf(buf, sizeof(buf), "* %d [%s] (voted) \n", i, players[i].name);
				} else if (players[i].is_alive) {
					snprintf(buf, sizeof(buf), "* %d [%s]\n", i, players[i].name);
				} else {
					snprintf(buf, sizeof(buf), "* %d [%s] (dead)\n", i, players[i].name);
				}
				write(players[pid].fd, buf, strlen(buf));
			}

			snprintf(buf, sizeof(buf), "Commands: /vote [id], /skip, /list\n");
			write(players[pid].fd, buf, strlen(buf));
		} else if (strncmp(input, "/kick", 5) == 0) {
			if (!players[pid].is_admin) {
				snprintf(buf, sizeof(buf), "You have no kicking permission\n");
				write(players[pid].fd, buf, strlen(buf));
			}

			char *endptr = NULL;
			strncpy(temp, &input[6], 4);
			printf("Decoding '%s' now\n", temp);
			vote = strtol(temp, &endptr, 10);
			if (!endptr || endptr[0] != '\0') {
				snprintf(buf, sizeof(buf), "Invalid kick, not an integer\n");
				write(players[pid].fd, buf, strlen(buf));
				return;
			}
			snprintf(buf, sizeof(buf), "Admin kicked [%s]\n", players[vote].name);
			broadcast(buf, -1);
			
			close(players[vote].fd);
			FD_CLR(players[vote].fd, &afds);
			players[vote].fd = -1;
			players[vote].is_alive = 0;
			goto check_votes;

		} else {
			snprintf(buf, sizeof(buf), "Invalid command\n");
			write(players[pid].fd, buf, strlen(buf));
		}
	} else if (input[0] == '/') {
		if (state.chats_left == 0) {
			snprintf(buf, sizeof(buf), "No chats left, you can only vote now\n");
			write(players[pid].fd, buf, strlen(buf));
			return;
		}
		snprintf(buf, sizeof(buf), "(%d) [%s] %s", state.chats_left, players[pid].name, &input[1]);
		broadcast(buf, -1);
		state.chats_left--;
	} else {
		if (state.chats_left == 0) {
			snprintf(buf, sizeof(buf), "No chats left, you can only vote now\n");
			write(players[pid].fd, buf, strlen(buf));
			return;
		}
		snprintf(buf, sizeof(buf), "(%d) [%s] %s", state.chats_left, players[pid].name, input);
		broadcast(buf, -1);
		state.chats_left--;
	}
}

void
adventure(int pid, char* input)
{
	char buf[1024];
	const char *location;
	int task_id;
	int task_is_long;

	if (input[0] == 'e' || strncmp(input, "ls", 3) == 0) {
		enum player_location loc = players[pid].location;
		strcpy(buf, "you can move to: ");
		assert(doors[loc][0] != -1);
		strncat(buf, locations[doors[loc][0]], sizeof(buf) - 1);
		for (size_t i = 1; doors[loc][i] != -1; i++) {
			strncat(strncat(buf, ", ", sizeof(buf) - 1),
				locations[doors[loc][i]], sizeof(buf) - 1);
		}
		strncat(buf, "\n", sizeof(buf) - 1);
		location = descriptions[loc];
		if (loc == LOC_REACTOR && state.is_reactor_meltdown) {
			location = "You are in the reactor room, there are red warning lights on and a siren is going off.\n";
		}
		write(players[pid].fd, location, strlen(location));
		write(players[pid].fd, buf, strlen(buf));
		for(int i=0; i<NUM_PLAYERS;i++) {
			if (players[i].location != players[pid].location
					|| players[i].fd == -1 || i == pid)
				continue;

			snprintf(buf, sizeof(buf),
					"you also see %s in the room with you\n",
					players[i].name);
			write(players[pid].fd, buf, strlen(buf));
		}
		snprintf(buf, sizeof(buf), "# ");
	} else if (strncmp(input, "go ", 3) == 0 || strncmp(input, "cd ", 3) == 0) {
		enum player_location new;
		for (new = 0; new < LOC_COUNT; new++) {
			if (strcmp(locations[new], &input[3]) == 0) {
				break;
			}
		}
		if (new == LOC_COUNT) {
			snprintf(buf, sizeof(buf), "INVALID MOVEMENT\n# ");
		} else {
			for (size_t i = 0; doors[players[pid].location][i] != -1; i++) {
				if (doors[players[pid].location][i] == new) {
					player_move(pid, new);
					snprintf(buf, sizeof(buf),
						"successfully moved\n# ");
					new = LOC_COUNT;
					break;
				}
			}
			if (new != LOC_COUNT) {
				snprintf(buf, sizeof(buf), "INVALID MOVEMENT\n# ");
			}
		}
	} else if (strcmp(input, "murder crewmate") == 0) {
		if (players[pid].is_imposter == 0) {
			snprintf(buf, sizeof(buf), "you might dislike them, but you can't kill them without weapon\n# ");
		} else if (players[pid].has_cooldown) {
			snprintf(buf, sizeof(buf), "you can't kill that quickly\n# ");
		} else {
			for(int i=0; i<NUM_PLAYERS;i++) {
				if (players[i].location != players[pid].location
						|| i == pid || players[i].fd == -1
						|| players[i].is_alive == 0)
					continue;

				// TODO: kill more randomly
				player_kill(pid, i);
				snprintf(buf, sizeof(buf), "you draw your weapon and brutally murder %s\n# ",
					players[i].name);
				break;
			}
		}
	} else if (strcmp(input, "report") == 0) {
		for(int i=0; i<NUM_PLAYERS;i++) {
			if (players[i].location != players[pid].location
					|| i == pid || players[i].fd == -1
					|| players[i].is_alive == 1)
				continue;

			start_discussion(pid, i);
			return;
		}

		snprintf(buf, sizeof(buf), "Nothing to report here\n# ");
	} else if (strcmp(input, "press emergency button") == 0) {
		if (players[pid].location != LOC_CAFETERIA) {
			snprintf(buf, sizeof(buf), "You can't do that here");
		} else {
			start_discussion(pid, -1);
			return;
		}
	} else if (strcmp(input, "check tasks") == 0) {
		player_list_tasks(pid);
		return;
	} else if (strcmp(input, "help") == 0) {
		snprintf(buf, sizeof(buf), "Commands: help, examine room, go [room], murder crewmate, report, check tasks\n# ");
	} else {
		// check if it was a task
		task_id = -1;
		task_is_long = 0;
		for(int i=0;i<TASK_SHORT_COUNT;i++) {
			if(strcmp(input, short_task_descriptions[i]) == 0) {
				task_id = i;
				break;
			}
		}
		for(int i=0;i<TASK_LONG_COUNT;i++) {
			if(strcmp(input, long_task_descriptions[i]) == 0) {
				task_id = i;
				task_is_long = 1;
				break;
			}
		}
		if (task_id == -1) {
			snprintf(buf, sizeof(buf), "Invalid instruction\n# ");
		} else {
			// check it was in the right room
			if (strstr(input, locations[players[pid].location]) != NULL) {
				task_completed(pid, task_id, task_is_long);
				snprintf(buf, sizeof(buf), "Completed task\n# ");
			} else {
				snprintf(buf, sizeof(buf), "You're in the wrong place for that\n# ");
			}
		}
	}
	write(players[pid].fd, buf, strlen(buf));
}

void
start_game()
{
	int imposternum;
	int assigned;
	char buf[200];
	int temp;

	broadcast("---------- [ Game is starting ] ----------", -1);
	state.stage = STAGE_PLAYING;	
	state.players = 0;
	for(int i=0; i<NUM_PLAYERS; i++) {
		if(players[i].fd != -1) {
			state.players++;
		}
	}

	// Pick an imposter
	imposternum = rand() % state.players;
	assigned = 0;
	for(int i=0; i<NUM_PLAYERS; i++) {
		if(players[i].fd == -1)
			continue;

		players[i].stage = PLAYER_STAGE_MAIN;
		players[i].location = LOC_CAFETERIA;
		players[i].is_alive = 1;

		// Assign NUM_SHORT random short tasks
		for(int j=0;j<NUM_SHORT;j++) {
retry:
			temp = rand() % TASK_SHORT_COUNT;
			for(int k=0;k<NUM_SHORT;k++) {
				if(players[i].short_tasks[k] == temp)
					goto retry;
			}
			players[i].short_tasks[j] = temp;
			players[i].short_tasks_done[j] = 0;
		}

		// Assign NUM_LONG random long tasks
		for(int j=0;j<NUM_LONG;j++) {
retry2:
			temp = rand() % TASK_LONG_COUNT;
			for(int k=0;k<NUM_LONG;k++) {
				if(players[i].long_tasks[k] == temp)
					goto retry2;
			}
			players[i].long_tasks[j] = temp;
			players[i].long_tasks_done[j] = 0;
		}

		if (assigned == imposternum) {
			players[i].is_imposter = 1;
			snprintf(buf, sizeof(buf), "You are the imposter, kill everyone without getting noticed.\n");
		} else {
			players[i].is_imposter = 0;
			snprintf(buf, sizeof(buf), "You are a crewmate, complete your tasks before everyone is killed.\n");
		}
		write(players[i].fd, buf, strlen(buf));
		assigned++;
	}


	// dramatic pause
	for(int i=0;i<5;i++) {
		sleep(1);
		broadcast(".", -1);
	}

	for(int i=0;i<NUM_PLAYERS;i++) {
		if(players[i].fd == -1)
			continue;

		if(players[i].is_imposter) {
			snprintf(buf, sizeof(buf), "You are in a spaceship, the other %d crew members think you're on of them\n# ", assigned - 1);
			write(players[i].fd, buf, strlen(buf));
		} else {
			snprintf(buf, sizeof(buf), "You are in a spaceship, one of the crew of %d people\n", assigned);
			write(players[i].fd, buf, strlen(buf));
			snprintf(buf, sizeof(buf), "The tasks have been handed out and the daily routine is starting up, but there are rumors one of your fellow crewmates isn't a crewmate at all.\n# ");
			write(players[i].fd, buf, strlen(buf));
		}
	}
}

int
handle_input(int fd)
{
	char buf[200];
	char buf2[300];
	int len;
	int pid;

	// Find player for fd
	for (pid = 0; pid < NUM_PLAYERS; pid++) {
		if (players[pid].fd == fd) {
			break;
		}
	}

	// Get the input
	len = read(fd, buf, 200);
	if (len < 0) {
		printf("Read error from player %d\n", pid);
		players[pid].fd = -1;
		return -1;
	}
	if (len == 0) {
		printf("Received EOF from player %d\n", pid);
		players[pid].fd = -1;
		if (players[pid].stage != PLAYER_STAGE_NAME) {
			snprintf(buf, sizeof(buf), "Player [%s] left the game.", players[pid].name);
			printf("Sending parting message\n");
			broadcast(buf, -1);
		}
		return -2;
	}

	for(int i=0; i<sizeof(buf); i++) {
		if (buf[i] == '\n' || buf[i] == '\r') {
			buf[i] = '\0';
			break;
		}
	}
	
	printf("%d: %s\n", pid, buf);

	switch(players[pid].stage) {
		case PLAYER_STAGE_NAME:
			// Setting the name after connection and informing the lobby
			if(strlen(buf) < MIN_NAME) {
				snprintf(buf, sizeof(buf), "Too short, pick another name\n >");
				write(fd, buf, strlen(buf));
				return 0;
			}
			if(strlen(buf) > MAX_NAME) {
				snprintf(buf, sizeof(buf), "Too long, pick another name\n >");
				write(fd, buf, strlen(buf));
				return 0;
			}
			for(int i=0;i<strlen(buf);i++){
				if(!isascii(buf[i])) {
					snprintf(buf, sizeof(buf), "Invalid char, pick another name\n >");
					write(fd, buf, strlen(buf));
					return 0;
				}
			}
			strcpy(players[pid].name, buf);

			snprintf(buf, sizeof(buf), "[%s] has joined the lobby", players[pid].name);
			broadcast(buf, fd);
			players[pid].stage = PLAYER_STAGE_LOBBY;
			break;

		case PLAYER_STAGE_LOBBY:
			// Chat message in the lobby
			if (buf[0] == '/') {
				if (strcmp(buf, "/start") == 0) {
					if(players[pid].is_admin) {
						start_game();
					} else {
						const char *msg = "You don't have permission to /start\n";
						write(fd, msg, strlen(msg));
					}
				} else if (strcmp(buf, "/shrug") == 0) {
					snprintf(buf2, sizeof(buf2), "[%s] ¯\\_(ツ)_/¯", players[pid].name);
					broadcast(buf2, fd);
				} else if (strncmp(buf, "/me ", 4) == 0) {
					snprintf(buf2, sizeof(buf2), " * [%s] %s", players[pid].name, &buf[4]);
					broadcast(buf2, fd);
				} else if (strcmp(buf, "/help") == 0) {
					snprintf(buf, sizeof(buf), "Commands: /start, /list and more\n");
					write(fd, buf, strlen(buf));
				} else if (strcmp(buf, "/list") == 0) {
					for(int i=0;i<NUM_PLAYERS;i++){
						if (players[i].fd == -1)
							continue;
						if (players[i].stage == PLAYER_STAGE_NAME) {
							snprintf(buf, sizeof(buf), " %d: -[ setting name ]-\n", i);
						} else if (players[i].is_admin) {
							snprintf(buf, sizeof(buf), " %d: %s (admin)\n", i, players[i].name);
						} else {
							snprintf(buf, sizeof(buf), " %d: %s\n", i, players[i].name);
						}
						write(fd, buf, strlen(buf));
					}
				}
			} else {
				for(int i=0;i<strlen(buf);i++){
					if(!isascii(buf[i])) {
						buf[i] = '\0';
					}
				}
				snprintf(buf2, sizeof(buf2), "[%s]: %s", players[pid].name, buf);
				broadcast(buf2, fd);
			}
			break;
		case PLAYER_STAGE_MAIN:
			// Main game adventure loop
			adventure(pid, buf);
			break;
		case PLAYER_STAGE_DISCUSS:
			// Main discussion loop
			discussion(pid, buf);
			break;
	}

	return 0;
}

int
welcome_player(int fd)
{
	int i;
	char buf[100];

	if(state.stage != STAGE_LOBBY) {
		snprintf(buf, sizeof(buf), "There is a game in progress, try again later\n");
		write(fd, buf, strlen(buf));
		close(fd);
		return -1;
	}

	for (i = 0; i < sizeof(players); i++) {
		if (players[i].fd > 0) {
			continue;
		}
		players[i].fd = fd;
		if (!state.has_admin) {
			state.has_admin = 1;
			players[i].is_admin = 1;
		}
		players[i].stage = PLAYER_STAGE_NAME;
		snprintf(buf, sizeof(buf), "Welcome player %d!\n\nEnter your name:\n> ", i);
		write(fd, buf, strlen(buf));
		printf("Assigned player to spot %d\n", i);
		return 0;
	}
	snprintf(buf, sizeof(buf), "There are no spots available, goodbye!\n");
	write(fd, buf, strlen(buf));
	close(fd);
	return -1;
}

int
main(void)
{
	int listen_fd;
	int new_fd;
	socklen_t client_size;
	struct sockaddr_in listen_addr, client_addr;
	int port = 1234;
	int i;

	for (i = 0; i < NUM_PLAYERS; i++) {
		players[i].fd = -1;
	};
	srand(time(NULL));

	listen_fd = socket(AF_INET, SOCK_STREAM, 0);

	listen_addr.sin_family = AF_INET;
	listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	listen_addr.sin_port = htons(port);

	if (bind(listen_fd, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) < 0) {
		perror("bind");
		return -1;
	}

	listen(listen_fd, 5);
	printf("Listening on :%d\n", port);

	state.stage = STAGE_LOBBY;

	FD_ZERO(&afds);
	FD_SET(listen_fd, &afds);

	while (1) {
		rfds = afds;
		if (select(FD_SETSIZE, &rfds, NULL, NULL, NULL) < 0) {
			perror("select");
			exit(EXIT_FAILURE);
		}

		for (i = 0; i < FD_SETSIZE; ++i) {
			if (FD_ISSET (i, &rfds)) {
				if (i == listen_fd) {
					printf("welcome client!\n");
					client_size = sizeof(client_addr);
					new_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_size);
					if (new_fd < 0) {
						perror("accept");
						exit(EXIT_FAILURE);
					}

					printf("New connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
					FD_SET(new_fd, &afds);
					if(welcome_player(new_fd)<0){
						FD_CLR(new_fd, &afds);
					}
				} else {
					if(handle_input(i) < 0) {
						close(i);
						FD_CLR(i, &afds);
					}
				}
			}
		}
	}
	return 0;
}
