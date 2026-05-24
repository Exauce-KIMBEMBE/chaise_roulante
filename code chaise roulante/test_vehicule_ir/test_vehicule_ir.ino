/***********************************************************************
 * Projet : Véhicule intelligent ESP32 piloté par télécommande IR
 *
 * Description :
 * Ce programme contrôle un véhicule basé sur ESP32 à l'aide
 * d'une télécommande infrarouge (protocole NEC).
 *
 * Fonctionnalités :
 * - Contrôle des déplacements (avant, arrière, gauche, droite)
 * - Gestion du klaxon
 * - Contrôle des feux avant et arrière
 * - Clignotants dynamiques pendant les virages
 * - Détection d'obstacles par capteurs ultrason
 * - Système d'alerte avec bip et signaux lumineux
 * - Arrêt automatique pour éviter les collisions
 * - Gestion des touches maintenues de la télécommande
 *
 * Développé par : Exaucé KIMBEMBE
 * Pseudo        : @OpenProgramming
 *
 * Date          : 24 Mai 2026
 * Version       : 1.0
 *
 * Plateforme    : ESP32
 * Bibliothèques :
 * - IRremote
 *
 ***********************************************************************/

#define DECODE_NEC
#include <IRremote.hpp>

#define IR_PIN 25

#define TRIG_AVANT 22
#define ECHO_AVANT 21

#define TRIG_ARRIERE 18
#define ECHO_ARRIERE 19

#define DISTANCE_DANGER 15

#define MG_IN1 12
#define MG_IN2 14
#define MD_IN1 27
#define MD_IN2 26

#define KLAXON_PIN 1      // TX
#define LED_AVANT 23

#define LAMPE_JAUNE_GAUCHE 13
#define LAMPE_JAUNE_DROITE 3   // RX

#define LAMPE_ROUGE_GAUCHE 33
#define LAMPE_ROUGE_DROITE 32

#define CMD_AVANT   0x18
#define CMD_ARRIERE 0x52
#define CMD_GAUCHE  0x08
#define CMD_DROITE  0x5A
#define CMD_KLAXON  0x1C
#define CMD_CH      0x46
#define CMD_PAUSE   0x43

bool ledAvantEtat = false;
bool feuxRougesEtat = false;

bool clignotantActif = false;
bool clignotantGauche = false;

bool alerteAvant = false;
bool alerteArriere = false;
bool alarmeMuette = false;

bool etatBlink = false;

uint16_t derniereCommande = 0;

unsigned long dernierSignalMoteur = 0;
unsigned long dernierSignalKlaxon = 0;
unsigned long dernierSignalVirage = 0;
unsigned long dernierBlink = 0;
unsigned long dernierControleObstacle = 0;

const unsigned long timeoutMoteur = 300;
const unsigned long timeoutKlaxon = 300;
const unsigned long timeoutVirage = 300;

const unsigned long intervalBlink = 150;
const unsigned long intervalControleObstacle = 200;

// Lit les commandes de la télécommande IR.
//
// Rôle :
// - Décode les signaux NEC reçus.
// - Gère les touches maintenues.
// - Envoie la commande à traiterCommande().
void lireTelecommande();

// Traite une commande de la télécommande.
//
// Paramètre :
// cmd : code reçu depuis la télécommande
//
// Actions possibles :
// - avancer
// - reculer
// - tourner
// - klaxon
// - LED avant
// - feux rouges
void traiterCommande(uint16_t cmd);

// Mesure la distance avec un capteur ultrason.
//
// Paramètres :
// trigPin : pin TRIG
// echoPin : pin ECHO
//
// Retour :
// Distance en cm
// Retourne -1 si mesure invalide
float mesurerDistance(int trigPin, int echoPin);

// Vérifie s'il existe un obstacle devant.
//
// Retour :
// true = danger détecté
// false = passage libre
bool dangerAvant();

// Vérifie s'il existe un obstacle derrière.
//
// Retour :
// true = danger détecté
// false = passage libre
bool dangerArriere();

// Gère les alertes obstacle.
//
// Rôle :
// - Fait clignoter les LEDs
// - Active le bip
// - Arrête l'alerte si l'objet disparaît
void gererAlerteObstacle();

// Arrête complètement l'alerte.
//
// Rôle :
// - Coupe le bip
// - Désactive les états d'alerte
// - Restaure les LEDs
void arreterAlerte();

// Déplace le véhicule vers l'avant.
void avancer();

// Déplace le véhicule vers l'arrière.
void reculer();

// Fait tourner le véhicule à gauche.
void tournerGauche();

// Fait tourner le véhicule à droite.
void tournerDroite();

// Arrête complètement les moteurs.
void stopMoteurs();

// Active un clignotant.
//
// Paramètre :
// gauche :
// true = gauche
// false = droite
void activerClignotant(bool gauche);

// Gère l'animation des clignotants.
//
// Rôle :
// - Produit le clignotement ON/OFF
// - Synchronise LED jaune et rouge
void gererClignotants();

// Éteint les clignotants jaunes.
void eteindreJaunes();

// Applique l'état mémorisé des feux rouges.
//
// Rôle :
// - Allume ou éteint les feux rouges
// selon feuxRougesEtat
void appliquerFeuxRouges();


void setup() {
  pinMode(IR_PIN, INPUT_PULLUP);

  pinMode(TRIG_AVANT, OUTPUT);
  pinMode(ECHO_AVANT, INPUT);

  pinMode(TRIG_ARRIERE, OUTPUT);
  pinMode(ECHO_ARRIERE, INPUT);

  pinMode(MG_IN1, OUTPUT);
  pinMode(MG_IN2, OUTPUT);
  pinMode(MD_IN1, OUTPUT);
  pinMode(MD_IN2, OUTPUT);

  pinMode(KLAXON_PIN, OUTPUT);
  pinMode(LED_AVANT, OUTPUT);

  pinMode(LAMPE_JAUNE_GAUCHE, OUTPUT);
  pinMode(LAMPE_JAUNE_DROITE, OUTPUT);
  pinMode(LAMPE_ROUGE_GAUCHE, OUTPUT);
  pinMode(LAMPE_ROUGE_DROITE, OUTPUT);

  stopMoteurs();

  digitalWrite(KLAXON_PIN, LOW);
  digitalWrite(LED_AVANT, LOW);

  eteindreJaunes();
  appliquerFeuxRouges();

  IrReceiver.begin(IR_PIN, DISABLE_LED_FEEDBACK);
}

void loop() {
  lireTelecommande();

  gererClignotants();
  gererAlerteObstacle();

  if (millis() - dernierSignalMoteur > timeoutMoteur) {
    stopMoteurs();
  }

  if (!alerteAvant && !alerteArriere) {
    if (millis() - dernierSignalKlaxon > timeoutKlaxon) {
      digitalWrite(KLAXON_PIN, LOW);
    }
  }

  if (clignotantActif && millis() - dernierSignalVirage > timeoutVirage) {
    clignotantActif = false;
    eteindreJaunes();
    appliquerFeuxRouges();
  }
}

void lireTelecommande() {
  if (IrReceiver.decode()) {
    uint16_t cmd = IrReceiver.decodedIRData.command;
    uint8_t flags = IrReceiver.decodedIRData.flags;

    if (cmd != 0) {
      derniereCommande = cmd;
      traiterCommande(cmd);
    } else if (flags & IRDATA_FLAGS_IS_REPEAT) {
      if (derniereCommande != 0) {
        traiterCommande(derniereCommande);
      }
    }

    IrReceiver.resume();
  }
}

void traiterCommande(uint16_t cmd) {
  if (cmd == CMD_AVANT) {
    if (dangerAvant()) {
      stopMoteurs();
      alerteAvant = true;
      alerteArriere = false;
      alarmeMuette = false;
    } else {
      arreterAlerte();
      avancer();
      dernierSignalMoteur = millis();
    }
  }

  else if (cmd == CMD_ARRIERE) {
    if (dangerArriere()) {
      stopMoteurs();
      alerteArriere = true;
      alerteAvant = false;
      alarmeMuette = false;
    } else {
      arreterAlerte();
      reculer();
      dernierSignalMoteur = millis();
    }
  }

  else if (cmd == CMD_GAUCHE) {
    arreterAlerte();

    tournerGauche();
    activerClignotant(true);

    dernierSignalVirage = millis();
    dernierSignalMoteur = millis();
  }

  else if (cmd == CMD_DROITE) {
    arreterAlerte();

    tournerDroite();
    activerClignotant(false);

    dernierSignalVirage = millis();
    dernierSignalMoteur = millis();
  }

  else if (cmd == CMD_KLAXON) {
    if (alerteAvant || alerteArriere) {
      alarmeMuette = true;
      digitalWrite(KLAXON_PIN, LOW);
    } else {
      digitalWrite(KLAXON_PIN, HIGH);
      dernierSignalKlaxon = millis();
    }
  }

  else if (cmd == CMD_CH) {
    ledAvantEtat = !ledAvantEtat;
    digitalWrite(LED_AVANT, ledAvantEtat);
  }

  else if (cmd == CMD_PAUSE) {
    feuxRougesEtat = !feuxRougesEtat;

    if (!clignotantActif && !alerteAvant && !alerteArriere) {
      appliquerFeuxRouges();
    }
  }
}

// ==================== ULTRASON ====================
float mesurerDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(5);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);

  digitalWrite(trigPin, LOW);

  long duree = pulseIn(echoPin, HIGH, 25000);

  if (duree == 0) {
    return -1;
  }

  float distance = duree * 0.0343 / 2.0;

  if (distance < 2 || distance > 200) {
    return -1;
  }

  return distance;
}

bool dangerAvant() {
  float distance = mesurerDistance(TRIG_AVANT, ECHO_AVANT);
  return distance > 0 && distance <= DISTANCE_DANGER;
}

bool dangerArriere() {
  float distance = mesurerDistance(TRIG_ARRIERE, ECHO_ARRIERE);
  return distance > 0 && distance <= DISTANCE_DANGER;
}

// ==================== ALERTE OBSTACLE ====================
void gererAlerteObstacle() {
  if (!alerteAvant && !alerteArriere) {
    return;
  }

  if (millis() - dernierControleObstacle >= intervalControleObstacle) {
    dernierControleObstacle = millis();

    if (alerteAvant && !dangerAvant()) {
      arreterAlerte();
      return;
    }

    if (alerteArriere && !dangerArriere()) {
      arreterAlerte();
      return;
    }
  }

  if (millis() - dernierBlink >= intervalBlink) {
    dernierBlink = millis();
    etatBlink = !etatBlink;
  }

  if (alarmeMuette) {
    digitalWrite(KLAXON_PIN, LOW);
  } else {
    digitalWrite(KLAXON_PIN, etatBlink);
  }

  if (alerteAvant) {
    digitalWrite(LAMPE_JAUNE_GAUCHE, etatBlink);
    digitalWrite(LAMPE_JAUNE_DROITE, etatBlink);

    digitalWrite(LAMPE_ROUGE_GAUCHE, feuxRougesEtat);
    digitalWrite(LAMPE_ROUGE_DROITE, feuxRougesEtat);
  }

  if (alerteArriere) {
    eteindreJaunes();

    digitalWrite(LAMPE_ROUGE_GAUCHE, etatBlink);
    digitalWrite(LAMPE_ROUGE_DROITE, etatBlink);
  }
}

void arreterAlerte() {
  alerteAvant = false;
  alerteArriere = false;
  alarmeMuette = false;

  digitalWrite(KLAXON_PIN, LOW);

  eteindreJaunes();
  appliquerFeuxRouges();
}

// ==================== MOTEURS ====================
void avancer() {
  digitalWrite(MG_IN1, HIGH);
  digitalWrite(MG_IN2, LOW);

  digitalWrite(MD_IN1, HIGH);
  digitalWrite(MD_IN2, LOW);
}

void reculer() {
  digitalWrite(MG_IN1, LOW);
  digitalWrite(MG_IN2, HIGH);

  digitalWrite(MD_IN1, LOW);
  digitalWrite(MD_IN2, HIGH);
}

void tournerGauche() {
  digitalWrite(MG_IN1, LOW);
  digitalWrite(MG_IN2, HIGH);

  digitalWrite(MD_IN1, HIGH);
  digitalWrite(MD_IN2, LOW);
}

void tournerDroite() {
  digitalWrite(MG_IN1, HIGH);
  digitalWrite(MG_IN2, LOW);

  digitalWrite(MD_IN1, LOW);
  digitalWrite(MD_IN2, HIGH);
}

void stopMoteurs() {
  digitalWrite(MG_IN1, LOW);
  digitalWrite(MG_IN2, LOW);

  digitalWrite(MD_IN1, LOW);
  digitalWrite(MD_IN2, LOW);
}

// ==================== CLIGNOTANTS ====================
void activerClignotant(bool gauche) {
  clignotantActif = true;
  clignotantGauche = gauche;
}

void gererClignotants() {
  if (!clignotantActif || alerteAvant || alerteArriere) {
    return;
  }

  if (millis() - dernierBlink >= intervalBlink) {
    dernierBlink = millis();
    etatBlink = !etatBlink;
  }

  if (clignotantGauche) {
    digitalWrite(LAMPE_JAUNE_GAUCHE, etatBlink);
    digitalWrite(LAMPE_ROUGE_GAUCHE, etatBlink);

    digitalWrite(LAMPE_JAUNE_DROITE, LOW);
    digitalWrite(LAMPE_ROUGE_DROITE, feuxRougesEtat);
  } else {
    digitalWrite(LAMPE_JAUNE_DROITE, etatBlink);
    digitalWrite(LAMPE_ROUGE_DROITE, etatBlink);

    digitalWrite(LAMPE_JAUNE_GAUCHE, LOW);
    digitalWrite(LAMPE_ROUGE_GAUCHE, feuxRougesEtat);
  }
}

// ==================== FEUX ====================
void eteindreJaunes() {
  digitalWrite(LAMPE_JAUNE_GAUCHE, LOW);
  digitalWrite(LAMPE_JAUNE_DROITE, LOW);
}

void appliquerFeuxRouges() {
  digitalWrite(LAMPE_ROUGE_GAUCHE, feuxRougesEtat);
  digitalWrite(LAMPE_ROUGE_DROITE, feuxRougesEtat);
}
