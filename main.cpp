#ifdef WINDOWS
  #define CLEAR "cls"
#else
  #define CLEAR "clear"
#endif

#include "stdio.h"
#include "stdlib.h"
#include "pthread.h"
#include "sys/time.h"
#include "semaphore.h"
#include "unistd.h"
#include "ncurses.h"
#include "stack"
#include "string"
#include "iostream"
#include "fstream"
#include "random"

// colors
#define RESET "\033[0m"
#define RED "\033[1m\033[31m"
#define GREEN "\033[1m\033[32m"
#define YELLOW "\033[1m\033[33m"
#define BLUE "\033[36m"
#define MAGENTA "\033[35m"
#define WHITE "\033[37m"

#define WEAPON_TYPES 4
#define NAMES 50
#define WEAPON_TYPE_NAME_SIZE 30
#define PLAYERS 10
#define ARENA_ENTRIES 10
#define TOTENS 1
#define WEAPON_MAX 10
#define SLEEP_TIME 2
#define LIFE_TOTAL 50
#define BARRIERS (PLAYERS)

#define enum { FALSE, TRUE }
#define DEBUG FALSE


typedef struct WeaponType {
  int damage;
  std::string name;
} WeaponType;

typedef struct Weapon {
  int id;
  WeaponType type;
} Weapon;

typedef struct {
  int id;
  std::string name;
  Weapon * current = NULL;
  int life = LIFE_TOTAL;
  int inArena = FALSE;
} Player;

int players_count = 0;
int game_ended = FALSE;
std::stack <Weapon> weapons;
WeaponType weapon_types[WEAPON_TYPES];
std::string names[NAMES];
Player* players[PLAYERS];

// LOCKS
pthread_mutex_t players_counter_lock; // control the counter for players
pthread_mutex_t totem_lock; // control the weapon totem stack
pthread_mutex_t weapon_types_lock; // control the weapon types array
pthread_mutex_t names_lock; // control the names array
pthread_mutex_t window_lock; // control the window for print
pthread_mutex_t players_lock[PLAYERS]; // control every player stats

// SEMAPHORES
sem_t weapon_totem_max; // control the maximum count a totem can have weapons
sem_t weapon_totem_available; // control the possibility to grab a weapon by players
sem_t arena; // control arena entrance
sem_t stop_totem;

// BARRIERS
pthread_barrier_t begin_game;
pthread_barrier_t end_game; // control the end game actions: when all players die, they reach the barrier, and when all die, the barrier breaks and the game ends

// FUNCTION SIGNATURES

int init_seed();
void init_weapon_types();
void init_names();
void * spawn_weapons();
void * spawn_players(void * args);
int last_player_standing(int life);
int shoot(Player player);
int targetable(int target_id, int player_id);
void print_scoreboard();

// FUNCTION IMPLEMENTATIONS

void * spawn_weapons(void * args) {
  int id = 0;
  Weapon weapon;

  srand(init_seed());

  while ( game_ended == FALSE ) {

    if(sem_trywait(&weapon_totem_max)) {
      weapon.id = id++;
      pthread_mutex_lock(&weapon_types_lock);
      weapon.type = weapon_types[ rand() % (WEAPON_TYPES) ];
      pthread_mutex_unlock(&weapon_types_lock);

      pthread_mutex_lock(&totem_lock);
      weapons.push(weapon);
      sem_post(&weapon_totem_available);
      pthread_mutex_unlock(&totem_lock);

      sleep(SLEEP_TIME / 2);
    }
  }
  pthread_exit(0);
}

void * spawn_players(void * args) {
  Player player;
  Weapon weapon;
  int *id = (int *) args;
  int name_pos;
  int player_alive;

  player.id = *id;

  pthread_mutex_lock(&names_lock);
  do {
    name_pos = rand() % (NAMES);
  } while (names[name_pos].empty());
  player.name.assign(names[name_pos]);
  names[name_pos].clear();
  pthread_mutex_unlock(&names_lock);

  pthread_mutex_lock(&players_lock[player.id]);
  players[player.id] = &player;
  pthread_mutex_unlock(&players_lock[player.id]);

  pthread_mutex_lock(&players_counter_lock);
  players_count++;
  pthread_mutex_unlock(&players_counter_lock);

  pthread_barrier_wait(&begin_game);

  // play()
  while ( TRUE ) {
    if (!player.current) {
      sem_wait(&weapon_totem_available);

      pthread_mutex_lock(&totem_lock);
      weapon = weapons.top();
      player.current = &weapon;
      weapons.pop();
      pthread_mutex_unlock(&totem_lock);

      if ( DEBUG ) {
        pthread_mutex_lock(&window_lock);
        printf("=== Player %d Created === \n", player.id);
        printf("Name : ");
        std::cout << player.name << "\n";
        printf("Weapon : ");
        std::cout << player.current->type.name << "\n";
        printf("======================== \n");
        pthread_mutex_unlock(&window_lock);
        sleep(SLEEP_TIME);
      }

      sem_post(&weapon_totem_max);
    }

    sem_wait(&arena);

    pthread_mutex_lock(&players_lock[player.id]);
    player.inArena = TRUE;
    pthread_mutex_unlock(&players_lock[player.id]);

    while(last_player_standing(player.id)) shoot(player);

    pthread_mutex_lock(&players_counter_lock);
    if (players_count == 1) {
      pthread_mutex_lock(&window_lock);
      std::cout << GREEN << player.name << " won the game\n" << RESET;
      pthread_mutex_unlock(&window_lock);
      sleep(SLEEP_TIME / 2);
      game_ended = TRUE;
    } else {
      players_count--;
      pthread_mutex_lock(&window_lock);
      std::cout << RED << player.name << " died\n" << RESET;
      pthread_mutex_unlock(&window_lock);
      sleep(SLEEP_TIME / 2);
    }
    pthread_mutex_unlock(&players_counter_lock);

    sem_post(&arena);
    player.inArena = FALSE;

    pthread_barrier_wait(&end_game);
    pthread_exit(0);
  }
}

int last_player_standing(int id) {

  pthread_mutex_lock(&players_lock[id]);
  int life = players[id]->life;
  pthread_mutex_unlock(&players_lock[id]);

  pthread_mutex_lock(&players_counter_lock);
  int count = players_count;
  pthread_mutex_unlock(&players_counter_lock);

  return ( life && (count > 1) ) ? TRUE : FALSE;
}

int shoot(Player player) {
  int target_id, damage, count;
  Player * target = NULL;

  do {
    target_id = rand() % PLAYERS;

    pthread_mutex_lock(&players_lock[target_id]);
    target = ( targetable(target_id, player.id) ) ? players[target_id] : NULL;
    pthread_mutex_unlock(&players_lock[target_id]);

    if ( DEBUG ) {
      pthread_mutex_lock(&window_lock);
      std::cout << "Target id: " << target_id << '\n';
      std::cout << "Player id: " << player.id << '\n';
      std::cout << "Targetable? : " << targetable(target_id, player.id) << '\n';
      std::cout << "Target NULL? " << (target == NULL) << '\n';
      sleep(SLEEP_TIME);
      pthread_mutex_unlock(&window_lock);
    }

    pthread_mutex_lock(&players_lock[player.id]);
    int life = players[player.id]->life;
    pthread_mutex_unlock(&players_lock[player.id]);
    if (!life) return FALSE;

    pthread_mutex_lock(&players_counter_lock);
    int count = players_count;
    pthread_mutex_unlock(&players_counter_lock);
    if (count == 1) return TRUE;

  } while (!target);

  pthread_mutex_lock(&window_lock);
  std::cout << BLUE << player.name << " defined a target\n" << RESET;
  pthread_mutex_unlock(&window_lock);
  sleep(SLEEP_TIME / 2);

  damage = (rand() % 2) ? player.current->type.damage : 0;

  if (damage) {

    pthread_mutex_lock(&players_lock[target_id]);
    target->life = ( damage >= target->life ) ? 0 : target->life - damage;
    pthread_mutex_unlock(&players_lock[target_id]);

    pthread_mutex_lock(&window_lock);
    std::cout << YELLOW << player.name << " dealt " << damage << " to " << target->name << "\n" RESET;
    pthread_mutex_unlock(&window_lock);
    sleep(SLEEP_TIME / 2);

    print_scoreboard();
    return TRUE;
  }

  pthread_mutex_lock(&window_lock);
  std::cout << WHITE << player.name << " missed the target\n" << RESET;
  pthread_mutex_unlock(&window_lock);
  sleep(SLEEP_TIME / 2);

  print_scoreboard();
  return FALSE;
}

int targetable(int target_id, int player_id) {
  return ( players[target_id]->inArena ) && ( players[target_id]->life ) && ( target_id != player_id );
}

int init_seed() {
  struct timeval time;
  gettimeofday(&time,NULL);
  return (time.tv_sec) + (time.tv_usec);
}

void init_names() {
  std::ifstream names_file;
  std::string name;

  names_file.open("names.txt");

  for (int i = 0; i < NAMES; i++) {
    std::getline(names_file, name);
    pthread_mutex_lock(&names_lock);
    names[i].assign(name);
    pthread_mutex_unlock(&names_lock);
  }

  names_file.close();
}

void init_weapon_types() {
  FILE * weapons_file;
  int damage;
  char name[WEAPON_TYPE_NAME_SIZE];

  weapons_file = fopen("weapon_types.txt", "r");

  for (int i = 0; i < WEAPON_TYPES; i++) {
    fscanf(weapons_file, "%d %[^\n]s", &damage, name);
    pthread_mutex_lock(&weapon_types_lock);
    weapon_types[i].damage = damage;
    weapon_types[i].name.assign(name);
    pthread_mutex_unlock(&weapon_types_lock);
  }

  fclose(weapons_file);
}

void print_scoreboard() {
  if ( DEBUG ) {
    pthread_mutex_lock(&window_lock);
    for (int i = 0 ; i < PLAYERS ; i++) {
      pthread_mutex_lock(&players_lock[i]);
      std::cout << WHITE << players[i]->name << "'s Life = " << players[i]->life << '\n' << RESET;
      pthread_mutex_unlock(&players_lock[i]);
      sleep(SLEEP_TIME / 4);
    }
    pthread_mutex_unlock(&window_lock);
  }
}

int main(int argc, char *argv[])
{
  int * id;
  pthread_t weapon_totem[TOTENS], players[PLAYERS], scoreboard;
  Weapon* weapon;

  sem_init(&weapon_totem_max, 0, WEAPON_MAX);
  sem_init(&weapon_totem_available, 0, 0);
  sem_init(&arena, 0, ARENA_ENTRIES);
  sem_init(&stop_totem, 0, 1);
  pthread_barrier_init(&begin_game, NULL, PLAYERS);
  pthread_barrier_init(&end_game, NULL, PLAYERS);

  system(CLEAR);

  init_weapon_types();
  init_names();
  srand(init_seed());

  for (int i = 0; i < TOTENS; i++) {
    pthread_create(&weapon_totem[i], NULL, spawn_weapons, NULL);
  }

  for (int i = 0; i < PLAYERS; i++) {
    id = (int *) malloc(sizeof(int));
    *id = i;

    pthread_create(&players[i], NULL, spawn_players, (void *) (id));
  }

  for (int i = 0 ; i < PLAYERS ; i++) pthread_join(players[i],NULL);

  for (int i = 0 ; i < TOTENS  ; i++) pthread_join(weapon_totem[i],NULL);

  while(1) {
    pthread_mutex_lock(&players_counter_lock);
    if (players_count == 0) {
      pthread_mutex_lock(&window_lock);
      std::cout << RED << "Everybody died. It's a tie\n" << RESET;
      pthread_mutex_unlock(&window_lock);
      sleep(SLEEP_TIME / 2);
      game_ended = TRUE;
      break;
    } else if ( game_ended == TRUE ) {
      break;
    }
    pthread_mutex_unlock(&players_counter_lock);
  }

  return 0;
}
