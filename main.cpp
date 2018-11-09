#ifdef WINDOWS
  #define CLEAR "cls"
#else
  #define CLEAR "clear"
#endif

#include "stdio.h"
#include "stdlib.h"
#include "pthread.h"
#include "unistd.h"
#include "ncurses.h"
#include "stack"
#include <string>
#include <iostream>

#define WEAPON_TYPES 4
#define WEAPON_TYPE_NAME_SIZE 30
#define PLAYERS 1
#define TOTENS 1

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
  Weapon* current = NULL;
  int life;
} Player;

int players_count = 1;
pthread_mutex_t players_lock;
pthread_mutex_t totem_lock;
pthread_mutex_t window_lock;
WeaponType weapon_types[WEAPON_TYPES];
std::stack <Weapon> weapons;

void init_weapon_types();
void * spawn_weapons();
void * spawn_players(void * args);

void * spawn_weapons() {
  int id = 0;
  time_t t;
  Weapon* weapon;

  srand((unsigned) time(&t));

  while ( TRUE ) {
    pthread_mutex_lock(&players_lock);
    if (!players_count) pthread_exit(0);
    pthread_mutex_unlock(&players_lock);
    weapon = (Weapon *) malloc(sizeof(Weapon));
    weapon->id = id++;
    weapon->type = weapon_types[rand() % WEAPON_TYPES - 1];
    pthread_mutex_lock(&window_lock);
    if ( DEBUG ) printf("ID DA ARMA: %d\n", weapon->id);
    if ( DEBUG ) std::cout << "TIPO DA ARMA: " << weapon->type.type << '\n';
    pthread_mutex_unlock(&window_lock);
    pthread_mutex_lock(&totem_lock);
    weapons.push(*weapon);
    pthread_mutex_unlock(&totem_lock);
    free(weapon);
    sleep(3);
  }
}

void * spawn_players(void * args) {
  Player* player;
  int *id = (int *) args;

  player = (Player *) malloc(sizeof(Player));
  player->id = *id;

  pthread_mutex_lock(&window_lock);
  std::cout << "Player " << player->id << " criado\n";
  pthread_mutex_unlock(&window_lock);

  free(player);

  while ( TRUE ) sleep(3);

  pthread_exit(0);
}

void init_weapon_types() {
  if ( DEBUG ) {
    FILE* weapons_file;
    int damage;
    char name[WEAPON_TYPE_NAME_SIZE];

    weapons_file = fopen("weapon_types.txt", "r");

    for (int i = 0; i < WEAPON_TYPES; i++) {
      fscanf(weapons_file, "%d %s", &damage, name);
      weapon_types[i].damage = damage;
      weapon_types[i].type.assign(name);
    }

    free(weapons_file);

  } else {
    weapon_types[0].damage = 10;
    weapon_types[0].type.assign("Shotgun");
    weapon_types[1].damage = 5;
    weapon_types[1].type.assign("Pistol");
    weapon_types[2].damage = 20;
    weapon_types[2].type.assign("Rifle");
    weapon_types[3].damage = 2;
    weapon_types[3].type.assign("Melee");
  }
}

int main(int argc, char *argv[])
{
  int *id;
  pthread_t weapon_totem[TOTENS], players[PLAYERS];
  Weapon* weapon;

  system(CLEAR);

  init_weapon_types();

  for (int i = 0; i < TOTENS; i++) {
    pthread_create(&weapon_totem[i], NULL, (void* (*)(void*)) spawn_weapons, NULL);
  }

  for (int i = 0; i < PLAYERS; i++) {
    id = (int *) malloc(sizeof(int));
    *id = i;

    pthread_create(&players[i], NULL, (void* (*)(void*)) spawn_players, (void *) (id));
    sleep(1);
    free(id);
  }

  while ( TRUE ) {
    if ( !DEBUG ) {
      pthread_mutex_lock(&totem_lock);
      if ( !weapons.empty() ) {
        weapon = (Weapon *) malloc(sizeof(Weapon));
        *weapon = weapons.top();
        printf("ID DA ARMA DO TOPO: %d\n", weapon->id);
        std::cout << "TIPO DA ARMA DO TOPO: " << weapon->type.type << '\n';
        free(weapon);
      }
      pthread_mutex_unlock(&totem_lock);
      sleep(1);
    }
  }

  return 0;
}
