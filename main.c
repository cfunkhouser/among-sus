#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>

#define NUM_PLAYERS 10
#define NUM_SHORT 6
#define NUM_LONG 2

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
	TASK_SHORT_COUNT
};

enum player_task_long {
	TASK_STORAGE_COUNT,
	TASK_O2_LOG,
	TASK_REACTOR_LOG,
	TASK_ADMIN_PIN,
	TASK_LONG_COUNT
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
};

struct player {
	int fd;
	struct sockaddr_in *addr;
	enum player_stage stage;
	char name[20];
	int is_admin;
	int is_imposter;
	enum player_location location;
	int in_vent;
	int is_alive;
	int has_cooldown;

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
};

struct gamestate state;
struct player *players;


void
broadcast(char* message, int notfd)
{
	char buf[1024];
	int pid;

	printf("*> %s\n", message);

	sprintf(buf, "%s\n", message);

	for (pid = 0; pid < NUM_PLAYERS; pid++) {
		if (players[pid].fd == -1)
			continue;
		if (players[pid].fd == notfd)
			continue;

		write(players[pid].fd, buf, strlen(buf));
	}
}

int
startswith(const char *str, const char *prefix)
{
	return strncmp(prefix, str, strlen(prefix)) == 0;
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
		if (i == pid)
			continue;
		if (players[i].fd == -1)
			continue;

		if (players[i].location != players[pid].location)
			continue;

		if (players[i].is_alive == 1)
			continue;

		sprintf(buf, "you enter the room and see the body of [%s] laying on the floor\n", players[i].name);
		write(players[pid].fd, buf, strlen(buf));
	}
}

void
player_list_tasks(int pid)
{
	char buf[100];
	char cm[2];

	for(int i=0;i<TASK_SHORT_COUNT;i++){
		for(int j=0;j<NUM_SHORT;j++) {
			if(players[pid].short_tasks[j] == i) {
				if(players[pid].short_tasks_done[j]) {
					sprintf(cm, "*");
				} else {
					sprintf(cm, " ");
				}
				switch (i) {
					case TASK_CAFE_TRASH:
						sprintf(buf, " [%s] Empty the cafeteria trash\n", cm);
						break;
					case TASK_CAFE_COFFEE:
						sprintf(buf, " [%s] Start the coffee maker in the cafeteria\n", cm);
						break;
					case TASK_CAFE_WIRES:
						sprintf(buf, " [%s] Fix cafeteria wireing\n", cm);
						break;
					case TASK_STORAGE_TRASH:
						sprintf(buf, " [%s] Empty the storage trash shute\n", cm);
						break;
					case TASK_ELECTRICAL_WIRES:
						sprintf(buf, " [%s] Fix wiring in electrical\n", cm);
						break;
					case TASK_ELECTRICAL_BREAKERS:
						sprintf(buf, " [%s] Reset breakers in electrical\n", cm);
						break;
					case TASK_ADMIN_WIRES:
						sprintf(buf, " [%s] Fix wiring in admin\n", cm);
						break;
					case TASK_NAVIGATION_WIRES:
						sprintf(buf, " [%s] Fix wiring in navigation\n", cm);
						break;
					case TASK_WEAPONS_WIRES:
						sprintf(buf, " [%s] Fix wiring in weapons\n", cm);
						break;
					case TASK_SHIELDS_WIRES:
						sprintf(buf, " [%s] Fix wiring in shields\n", cm);
						break;
					case TASK_O2_WIRES:
						sprintf(buf, " [%s] Fix wiring in o2\n", cm);
						break;
					case TASK_O2_CLEAN:
						sprintf(buf, " [%s] Clean oxygenator output in o2\n", cm);
						break;
					case TASK_MEDBAY_WIRES:
						sprintf(buf, " [%s] Fix wiring in medbay\n", cm);
						break;
					case TASK_UPPER_CATALYZER:
						sprintf(buf, " [%s] Check catalyzer in upper engine\n", cm);
						break;
					case TASK_LOWER_CATALYZER:
						sprintf(buf, " [%s] Check catalyzer in lower engine\n", cm);
						break;
					case TASK_UPPER_COMPRESSION_COIL:
						sprintf(buf, " [%s] Replace compression coil in upper engine\n", cm);
						break;
					case TASK_LOWER_COMPRESSION_COIL:
						sprintf(buf, " [%s] Replace compression coil in lower engine\n", cm);
						break;
				}
				write(players[pid].fd, buf, strlen(buf));
			}
		}
	}
}

void
player_kill(int pid, int tid)
{
	char buf[100];
	int crew_alive = 0;

	if(players[pid].location != players[tid].location)
		return;

	// so sad
	players[tid].is_alive = 0;

	// less murdering, reset by movement
	players[pid].has_cooldown = 1;
	
	// notify player of their recent death
	sprintf(buf, "It turns out %s is the imposter, sadly the way you know is that you died.\n", players[pid].name);
	write(players[tid].fd, buf, strlen(buf));

	// notify bystanders
	for(int i=0; i<NUM_PLAYERS;i++) {
		if (i == pid)
			continue;
		if (players[i].fd == -1)
			continue;
		if (players[i].is_alive == 0)
			continue;

		crew_alive++;

		if (players[i].location != players[pid].location)
			continue;

		sprintf(buf, "someone killed [%s] while you were in the room\n", players[tid].name);
		write(players[i].fd, buf, strlen(buf));
	}

	// Check win condition
	if(crew_alive == 1) {
		broadcast("The imposter won", -1);
	}
}

void
start_discussion(int pid, int bid)
{
	char buf[100];

	state.stage = STAGE_DISCUSS;

	// switch everyone to the discussion state
	for(int i=0; i<NUM_PLAYERS;i++) {
		if (players[i].fd == -1)
			continue;

		players[i].stage = PLAYER_STAGE_DISCUSS;
	}

	// Inform everyone
	if(bid == -1) {
		// Emergency button was pressed
		sprintf(buf, "An emergency meeting was called by [%s], discuss", players[pid].name);
	} else {
		// Body was reported
		sprintf(buf, "The body of [%s] was found by [%s], discuss", players[bid].name, players[pid].name);
	}
	broadcast(buf, -1);

	// List the state of the players
	broadcast("Players:", -1);
	for(int i=0; i<NUM_PLAYERS;i++) {
		if (players[i].fd == -1)
			continue;
		if (players[i].is_alive) {
			sprintf(buf, "* %d [%s]", i, players[i].name);
		} else {
			sprintf(buf, "* %d [%s] (dead)", i, players[i].name);
		}
		broadcast(buf, -1);
	}
}

void
discussion(int pid, char* input)
{
	char buf[300];
	if (buf[0] == '/') {
		if (startswith(buf, "/vote ")) {
		}
	} else {
		sprintf(buf, "[%s] %s", players[pid].name, buf);
		broadcast(buf, players[pid].fd);
	}
}

void
adventure(int pid, char* input)
{
	char buf[1024];
	char location[160];
	char doors[100];
	char other[100];
	if (strcmp(input, "examine room", 12) == 0 || strcmp(input, "e", 2) == 0) {
		switch(players[pid].location) {
			case LOC_CAFETERIA:
				sprintf(location, "You are standing in the middle of the cafeteria, in the center there's an emergency button\n");
				sprintf(doors, "you can move to: medbay, admin, weapons\n");
				break;
			case LOC_REACTOR:
				if (state.is_reactor_meltdown) {
					sprintf(location, "You are in the reactor room, there are red warning lights on and a siren is going off.\n");
				} else {
					sprintf(location, "You are in the reactor room, it seems to be running normally\n");
				}
				sprintf(doors, "you can move to: upper engine, security, lower engine\n");
				break;
			case LOC_UPPER_ENGINE:
			case LOC_LOWER_ENGINE:
				sprintf(location, "You are in a small room, mostly filled up by an engine.\n");
				sprintf(doors, "you can move to: reactor, electrical\n");
				break;
			case LOC_SECURITY:
				sprintf(location, "You are in a small room filled with monitors, the monitors are showing camera images showing an overview of the ship\n");
				sprintf(doors, "you can move to: upper engine, reactor, lower engine\n");
				break;
			case LOC_MEDBAY:
				sprintf(location, "You are in a room with beds and a medical scanner.\n");
				sprintf(doors, "you can move to: upper engine, cafeteria\n");
				break;
			case LOC_ELECTRICAL:
				sprintf(location, "You are in a room filled with equipment racks. Some of them have wires sticking out of them\n");
				sprintf(doors, "you can move to: lower engine, storage\n");
				break;
			case LOC_STORAGE:
				sprintf(location, "You are in a large room filled with boxes. One of the walls has a large door to the outside\n");
				sprintf(doors, "you can move to: electrical, admin, comms\n");
				break;
			case LOC_ADMIN:
				sprintf(location, "You are in a nice carpeted room with a holographic map in the middle\n");
				sprintf(doors, "you can move to: cafe, storage\n");
				break;
			case LOC_COMMUNICATIONS:
				sprintf(location, "You are in a small room with what looks like radio equipment\n");
				sprintf(doors, "you can move to: storage, shields\n");
				break;
			case LOC_O2:
				sprintf(location, "You are in a room with plants in terrariums and life support equipment\n");
				sprintf(doors, "you can move to: shields, weapons, navigation\n");
				break;
			case LOC_WEAPONS:
				sprintf(location, "You are in a circular room with a targeting system in the middle and a view of outer space\n");
				sprintf(doors, "you can move to: cafe, o2, navigation\n");
				break;
			case LOC_SHIELDS:
				sprintf(location, "You are in a circular room with glowing tubes and a control panel for the shields\n");
				sprintf(doors, "you can move to: storage, o2, navigation\n");
				break;
			case LOC_NAVIGATION:
				sprintf(location, "You are all the way in the front of the ship in a room with the ship controls and a great view of space\n");
				sprintf(doors, "you can move to: weapons, o2, shields\n");
				break;
		}
		write(players[pid].fd, location, strlen(location));
		write(players[pid].fd, doors, strlen(doors));
		for(int i=0; i<NUM_PLAYERS;i++) {
			if (i == pid)
				continue;

			if (players[i].fd == -1)
				continue;

			if (players[i].location != players[pid].location)
				continue;

			sprintf(other, "you also see %s in the room with you\n", players[i].name);
			write(players[pid].fd, other, strlen(other));
		}
		sprintf(buf, "# ");
	} else if (startswith(input, "go ")) {
		sprintf(buf, "\n# ");
		if (startswith(input, "go cafe")) {
			if (players[pid].location == LOC_MEDBAY ||
					players[pid].location == LOC_WEAPONS ||
					players[pid].location == LOC_ADMIN)
				player_move(pid, LOC_CAFETERIA);
		} else if (startswith(input, "go react")) {
			if (players[pid].location == LOC_SECURITY ||
					players[pid].location == LOC_UPPER_ENGINE ||
					players[pid].location == LOC_LOWER_ENGINE)
				player_move(pid, LOC_REACTOR);
		} else if (startswith(input, "go upper")) {
			if (players[pid].location == LOC_MEDBAY ||
					players[pid].location == LOC_SECURITY ||
					players[pid].location == LOC_REACTOR)
				player_move(pid, LOC_UPPER_ENGINE);
		} else if (startswith(input, "go lower")) {
			if (players[pid].location == LOC_ELECTRICAL ||
					players[pid].location == LOC_SECURITY ||
					players[pid].location == LOC_REACTOR)
				player_move(pid, LOC_LOWER_ENGINE);
		} else if (startswith(input, "go security")) {
			if (players[pid].location == LOC_REACTOR ||
					players[pid].location == LOC_LOWER_ENGINE ||
					players[pid].location == LOC_UPPER_ENGINE)
				player_move(pid, LOC_SECURITY);
		} else if (startswith(input, "go medbay")) {
			if (players[pid].location == LOC_UPPER_ENGINE ||
					players[pid].location == LOC_CAFETERIA)
				player_move(pid, LOC_MEDBAY);
		} else if (startswith(input, "go electrical")) {
			if (players[pid].location == LOC_LOWER_ENGINE ||
					players[pid].location == LOC_STORAGE)
				player_move(pid, LOC_ELECTRICAL);
		} else if (startswith(input, "go storage")) {
			if (players[pid].location == LOC_ELECTRICAL ||
					players[pid].location == LOC_ADMIN ||
					players[pid].location == LOC_CAFETERIA ||
					players[pid].location == LOC_COMMUNICATIONS)
				player_move(pid, LOC_STORAGE);
		} else if (startswith(input, "go admin")) {
			if (players[pid].location == LOC_CAFETERIA ||
					players[pid].location == LOC_STORAGE)
				player_move(pid, LOC_ADMIN);
		} else if (startswith(input, "go comm")) {
			if (players[pid].location == LOC_STORAGE ||
					players[pid].location == LOC_SHIELDS)
				player_move(pid, LOC_COMMUNICATIONS);
		} else if (startswith(input, "go o2")) {
			if (players[pid].location == LOC_WEAPONS ||
					players[pid].location == LOC_SHIELDS ||
					players[pid].location == LOC_NAVIGATION)
				player_move(pid, LOC_O2);
		} else if (startswith(input, "go weapon")) {
			if (players[pid].location == LOC_CAFETERIA ||
					players[pid].location == LOC_O2 ||
					players[pid].location == LOC_NAVIGATION)
				player_move(pid, LOC_WEAPONS);
		} else if (startswith(input, "go shield")) {
			if (players[pid].location == LOC_COMMUNICATIONS ||
					players[pid].location == LOC_O2 ||
					players[pid].location == LOC_NAVIGATION)
				player_move(pid, LOC_SHIELDS);
		} else if (startswith(input, "go nav")) {
			if (players[pid].location == LOC_WEAPONS ||
					players[pid].location == LOC_O2 ||
					players[pid].location == LOC_SHIELDS)
				player_move(pid, LOC_NAVIGATION);
		} else {	
			sprintf(buf, "INVALID MOVEMENT\n# ");
		}
	} else if (startswith(input, "murder crewmate")) {
		if (players[pid].is_imposter == 0) {
			sprintf(buf, "you might dislike him, but you can't kill him without weapon\n# ");
		} else if (players[pid].has_cooldown) {
			sprintf(buf, "you can't kill that quickly\n# ");
		} else {
			for(int i=0; i<NUM_PLAYERS;i++) {
				if (i == pid)
					continue;

				if (players[i].fd == -1)
					continue;

				if (players[i].location != players[pid].location)
					continue;

				if (players[i].is_alive == 0)
					continue;

				// TODO: kill more randomly
				player_kill(pid, i);
				sprintf(buf, "you draw your weapon and brutally murder %s\n# ", players[i].name);
				break;
			}
		}
	} else if (startswith(input, "report")) {
		for(int i=0; i<NUM_PLAYERS;i++) {
			if (i == pid)
				continue;

			if (players[i].fd == -1)
				continue;

			if (players[i].location != players[pid].location)
				continue;

			if (players[i].is_alive == 1)
				continue;

			start_discussion(pid, i);
			return;
		}

		sprintf(buf, "Nothing to report here\n# ");
	} else if (startswith(input, "check tasks")) {
		player_list_tasks(pid);
		return;
	} else if (strcmp(input, "help", 4) == 0) {
		sprintf(buf, "Commands: help, examine room, go [room], murder crewmate, report, check tasks\n# ");
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

		if (assigned == imposternum) {
			players[i].is_imposter = 1;
			sprintf(buf, "You are the imposter, kill everyone without getting noticed\n# ");
		} else {
			players[i].is_imposter = 0;
			sprintf(buf, "You are a crewmate, complete your tasks before everyone is killed\n# ");
		}
		write(players[i].fd, buf, strlen(buf));
		assigned++;
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
		return -1;
	}
	if (len == 0) {
		printf("Received EOF from player %d\n", pid);
		return -2;
	}

	for(int i=0; i<sizeof(buf); i++) {
		if (buf[i] == '\n') {
			buf[i] = '\0';
			break;
		}
	}
	
	printf("%d: %s\n", pid, buf);

	switch(players[pid].stage) {
		case PLAYER_STAGE_NAME:
			// Setting the name after connection and informing the lobby
			strcpy(players[pid].name, buf);
			sprintf(buf, "[%s] has joined the lobby", players[pid].name);
			broadcast(buf, fd);
			players[pid].stage = PLAYER_STAGE_LOBBY;
			break;

		case PLAYER_STAGE_LOBBY:
			// Chat message in the lobby
			if (buf[0] == '/') {
				if (strncmp(buf, "/start", 6) == 0) {
					if(players[pid].is_admin) {
						start_game();
					} else {
						sprintf(buf2, "You don't have permission to /start\n");
						write(fd, buf2, strlen(buf2));
					}
				}
			} else {
				sprintf(buf2, "[%s] %s", players[pid].name, buf);
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

void
welcome_player(int fd, struct sockaddr_in addr)
{
	int i;
	char buf[100];

	for (i = 0; i < sizeof(players); i++) {
		if (players[i].fd > 0) {
			continue;
		}
		// Bonus points for goto usage
		goto found_spot;	
	}
	sprintf(buf, "There are no spots available, goodbye!\n");
	write(fd, buf, strlen(buf));
	close(fd);
	return;

found_spot:
	printf("Assigned player to spot %d\n", i);
	players[i].fd = fd;
	players[i].addr = malloc(sizeof(addr));
	if (!state.has_admin) {
		state.has_admin = 1;
		players[i].is_admin = 1;
	}
	players[i].stage = PLAYER_STAGE_NAME;
	sprintf(buf, "Welcome player %d!\n\nEnter your name:\n> ", i);
	write(fd, buf, strlen(buf));
}

int
main()
{
	int listen_fd;
	int new_fd;
	int client_size;
	struct sockaddr_in listen_addr, client_addr;
	int port = 1234;
	int i;
	fd_set rfds, afds;

	players = (struct player*)malloc(sizeof(struct player) * NUM_PLAYERS);

	for (i = 0; i < NUM_PLAYERS; i++) {
		players[i].fd = -1;
	};
	srand(time(NULL));

	listen_fd = socket(AF_INET, SOCK_STREAM, 0);

	listen_addr.sin_family = AF_INET;
	listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	listen_addr.sin_port = htons(port);

	if (bind(listen_fd, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) < 0) {
		printf("bind failed\n");
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
			printf("select oops\n");
			exit(EXIT_FAILURE);
		}

		for (i = 0; i < FD_SETSIZE; ++i) {
			if (FD_ISSET (i, &rfds)) {
				if (i == listen_fd) {
					printf("welcome client!\n");
					client_size = sizeof(client_addr);
					new_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_size);
					if (new_fd < 0) {
						printf("new client oops\n");
						exit(EXIT_FAILURE);
					}

					printf("New connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
					FD_SET(new_fd, &afds);
					welcome_player(new_fd, client_addr);
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
