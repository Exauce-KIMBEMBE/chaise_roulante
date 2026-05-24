#include <IRremote.hpp>

#define IR_PIN 25

#define MG_IN1 12
#define MG_IN2 14
#define MD_IN1 27
#define MD_IN2 26

#define KLAXON_PIN 1   // TX

#define LED_AVANT 23

#define LAMPE_JAUNE_GAUCHE 13
#define LAMPE_JAUNE_DROITE 3   // RX

#define LAMPE_ROUGE_GAUCHE 33
#define LAMPE_ROUGE_DROITE 32

#define CMD_AVANT     0x18
#define CMD_ARRIERE   0x52
#define CMD_GAUCHE    0x08
#define CMD_DROITE    0x5A
#define CMD_KLAXON    0x1C
#define CMD_CH        0x46
#define CMD_PAUSE     0x43

bool ledAvantEtat = false;
bool feuxRougesEtat = false;

uint16_t derniereCommande = 0;

unsigned long dernierSignalMoteur = 0;
unsigned long dernierSignalKlaxon = 0;
unsigned long dernierSignalVirage = 0;

const unsigned long timeoutMoteur = 300;
const unsigned long timeoutKlaxon = 300;
const unsigned long timeoutVirage = 300;

bool clignotantActif = false;
bool clignotantGauche = false;
bool etatBlink = false;

unsigned long dernierBlink = 0;
const unsigned long intervalBlink = 150;

void setup() {
  pinMode(IR_PIN, INPUT_PULLUP);

  pinMode(MG_IN1, OUTPUT);
  pinMode(MG_IN2, OUTPUT);
  pinMode(MD_IN1, OUTPUT);
  pinMode(MD_IN2, OUTPUT);

  pinMode(KLAXON_PIN, OUTPUT);
digitalWrite(KLAXON_PIN, LOW);
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

  IrReceiver.begin(IR_PIN, ENABLE_LED_FEEDBACK);
}

void loop() {
  lireTelecommande();
  gererClignotants();

  if (millis() - dernierSignalMoteur > timeoutMoteur) {
    stopMoteurs();
  }

  if (millis() - dernierSignalKlaxon > timeoutKlaxon) {
    digitalWrite(KLAXON_PIN, LOW);
  }

  if (clignotantActif && millis() - dernierSignalVirage > timeoutVirage) {
    clignotantActif = false;
    eteindreJaunes();
    appliquerFeuxRouges();
  }
}

void lireTelecommande() {
  if (IrReceiver.decode()) {

    decode_type_t protocole = IrReceiver.decodedIRData.protocol;
    uint16_t cmd = IrReceiver.decodedIRData.command;
    uint8_t flags = IrReceiver.decodedIRData.flags;

    if (protocole == NEC && cmd != 0) {
      derniereCommande = cmd;
      traiterCommande(cmd);
    }

    else if (flags & IRDATA_FLAGS_IS_REPEAT) {
      if (derniereCommande != 0) {
        traiterCommande(derniereCommande);
      }
    }

    IrReceiver.resume();
  }
}

void traiterCommande(uint16_t cmd) {
  if (cmd == CMD_AVANT) {
    avancer();
    dernierSignalMoteur = millis();
  }

  else if (cmd == CMD_ARRIERE) {
    reculer();
    dernierSignalMoteur = millis();
  }

  else if (cmd == CMD_GAUCHE) {
    tournerGauche();
    activerClignotant(true);
    dernierSignalMoteur = millis();
    dernierSignalVirage = millis();
  }

  else if (cmd == CMD_DROITE) {
    tournerDroite();
    activerClignotant(false);
    dernierSignalMoteur = millis();
    dernierSignalVirage = millis();
  }

  else if (cmd == CMD_KLAXON) {
    digitalWrite(KLAXON_PIN, HIGH);
    dernierSignalKlaxon = millis();
  }

  else if (cmd == CMD_CH) {
    ledAvantEtat = !ledAvantEtat;
    digitalWrite(LED_AVANT, ledAvantEtat);
  }

  else if (cmd == CMD_PAUSE) {
    feuxRougesEtat = !feuxRougesEtat;

    if (!clignotantActif) {
      appliquerFeuxRouges();
    }
  }
}

// ---------- MOTEURS ----------
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

// ---------- CLIGNOTANTS ----------
void activerClignotant(bool gauche) {
  clignotantActif = true;
  clignotantGauche = gauche;
}

void gererClignotants() {
  if (!clignotantActif) return;

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

void eteindreJaunes() {
  digitalWrite(LAMPE_JAUNE_GAUCHE, LOW);
  digitalWrite(LAMPE_JAUNE_DROITE, LOW);
}

void appliquerFeuxRouges() {
  digitalWrite(LAMPE_ROUGE_GAUCHE, feuxRougesEtat);
  digitalWrite(LAMPE_ROUGE_DROITE, feuxRougesEtat);
}