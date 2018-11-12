#ifdef WINDOWS
  #define CLEAR "cls"
#else
  #define CLEAR "clear"
#endif

#include "stdio.h"
#include "stdlib.h"
#include "pthread.h"
#include <sys/time.h>
#include <semaphore.h>
#include "unistd.h"
#include "ncurses.h"
#include "stack"
#include <string>
#include <iostream>
#include <fstream>
#include <random>

#define WEAPON_TYPES 4
#define NAMES 50
#define WEAPON_TYPE_NAME_SIZE 30
#define PLAYERS 5
#define TOTENS 1
#define WEAPON_MAX 10
#define WEAPON_SPAWN_TIME .5

#define enum { FALSE, TRUE }
#define DEBUG TRUE


typedef struct WeaponType {
  int damage;
  std::string type;
} WeaponType;

typedef struct Weapon {
  int id;
  WeaponType type;
} Weapon;

typedef struct {
  int id;
  std::string name;
  Weapon * current = NULL;
  int life;
} Player;

int players_count = 1;
std::stack <Weapon> weapons;
WeaponType weapon_types[WEAPON_TYPES];
std::string names[NAMES];

// LOCKS
pthread_mutex_t players_lock; // control the counter for players
pthread_mutex_t totem_lock; // control the weapon totem stack
pthread_mutex_t weapon_types_lock; // control the weapon types array
pthread_mutex_t names_lock; // control the names array
pthread_mutex_t window_lock; // control the window for print

// SEMAPHORES
sem_t weapon_totem_max; // control the maximum count a totem can have weapons
sem_t weapon_totem_available; // control the possibility to grab a weapon by players


// FUNCTION SIGNATURES

void init_weapon_types();
void init_names();
void * spawn_weapons();
void * spawn_players(void * args);

// FUNCTION IMPLEMENTATIONS

void * spawn_weapons(void * args) {
  int id = 0;
  time_t t;
  Weapon weapon;

  srand((unsigned) time(&t));

  while ( TRUE ) {
    pthread_mutex_lock(&players_lock);
    if (!players_count) pthread_exit(0);
    pthread_mutex_unlock(&players_lock);

    if (!sem_wait(&weapon_totem_max)) {
      weapon.id = id++;
      pthread_mutex_lock(&weapon_types_lock);
      weapon.type = weapon_types[ rand() % (WEAPON_TYPES - 1) ];
      pthread_mutex_unlock(&weapon_types_lock);

      if ( !DEBUG ) {
        pthread_mutex_lock(&window_lock);
        printf("ID DA ARMA: %d\n", weapon.id);
        std::cout << "TIPO DA ARMA: " << weapon.type.type << '\n';
        pthread_mutex_unlock(&window_lock);
      }

      pthread_mutex_lock(&totem_lock);
      weapons.push(weapon);
      sem_post(&weapon_totem_available);
      pthread_mutex_unlock(&totem_lock);

      sleep(WEAPON_SPAWN_TIME);
    }
  }
}

void * spawn_players(void * args) {
  Player player;
  Weapon weapon;
  int *id = (int *) args;
  int name_pos;

  // init rand() seed
  struct timeval time;
  gettimeofday(&time,NULL);
  srand((time.tv_sec) + (time.tv_usec));

  player.id = *id;

  pthread_mutex_lock(&names_lock);

  do {
    name_pos = rand() % (NAMES);
  } while (names[name_pos].empty());
  player.name.assign(names[name_pos]);
  names[name_pos].clear();

  pthread_mutex_unlock(&names_lock);

  if ( DEBUG ) {
    pthread_mutex_lock(&window_lock);
    std::cout << "Player " << player.name << " criado\n";
    pthread_mutex_unlock(&window_lock);
  }

  while ( TRUE ) {
    if (!player.current) {
      sem_wait(&weapon_totem_available);
      pthread_mutex_lock(&totem_lock);
      /* std::cout << "Player current weapon" << player.current; */
      weapon = weapons.top();
      player.current = &weapon;
      weapons.pop();
      pthread_mutex_unlock(&totem_lock);
    }
    if ( DEBUG ) {
      pthread_mutex_lock(&window_lock);
      std::cout << "Player " << player.name << " pegou arma " << player.current->type.type << " com ID: " << player.current->id << '\n';
      pthread_mutex_unlock(&window_lock);
      sleep(10);
    }
  }

  pthread_exit(0);
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
    weapon_types[i].type.assign(name);
    pthread_mutex_unlock(&weapon_types_lock);
  }

  fclose(weapons_file);
}

int main(int argc, char *argv[])
{
  int * id;
  pthread_t weapon_totem[TOTENS], players[PLAYERS];
  Weapon* weapon;

  sem_init(&weapon_totem_max, 0, WEAPON_MAX);
  sem_init(&weapon_totem_available, 0, 0);

  system(CLEAR);

  init_weapon_types();
  init_names();

  for (int i = 0; i < TOTENS; i++) {
    pthread_create(&weapon_totem[i], NULL, spawn_weapons, NULL);
  }

  for (int i = 0; i < PLAYERS; i++) {
    id = (int *) malloc(sizeof(int));
    *id = i;

    pthread_create(&players[i], NULL, spawn_players, (void *) (id));
  }

  for (int i = 0; i < PLAYERS ; i++) pthread_join(players[i],NULL);

  for (int i = 0; i < TOTENS ; i++) pthread_join(weapon_totem[i],NULL);

  while ( TRUE );

  return 0;
}
