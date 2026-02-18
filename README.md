# S4_EspCom
Esp32 C6 N16, communication entre le esp vehicule et le Basys

📡 ESP-NOW – TX/RX minimal (FreeRTOS)
Ce projet envoie et reçoit des messages ESP-NOW entre deux ESP32 via FreeRTOS.
Chaque message fait 2 octets et contient : une vitesse (13 bits) et un ID (3 bits).
🔧 Pré-requis

ESP-IDF (v4.x ou v5.x)
Carte ESP32
Deux appareils si tu veux tester TX↔RX entre deux nœuds
Le module Wi‑Fi activé (ESP-NOW utilise le Wi‑Fi en mode direct)

🚀 Démarrage rapide


Configurer l’adresse MAC du pair (le récepteur si tu flashes l’émetteur) :
Dans main.cpp :
C++static const uint8_t peer_mac[6] = { 0x20, 0x6E, 0xF1, 0x09, 0xB3, 0xA0 };Afficher plus de lignes

Remplace par la MAC de l’autre ESP (affichée au boot).



Choisir le canal Wi‑Fi (les deux doivent être identiques) :
C++const uint8_t channel = 1;Afficher plus de lignes


Compiler & flasher :
Shellidf.py set-target esp32idf.py build flash monitor``Afficher plus de lignes


Vérifier les logs série :

Le noeud affiche sa MAC au démarrage.
Le RX imprime périodiquement le dernier ID, Vitesse, seq (numéro de séquence) et total (compteur de paquets).




🧠 Ce que fait chaque partie
app_main

Initialise la comm (com_init(channel)).
Affiche la MAC locale (com_get_mac).
Ajoute le pair pour l’émission (com_add_peer(peer_mac)).
Lance 2 tâches FreeRTOS :

tx_task (émission)
rx_task (réception)



tx_task – Émet à ~500 Hz

Période : 2 ms (pdMS_TO_TICKS(2)).
Prépare un entier 16 bits packed :

bits [15:3] → vitesse (13 bits, 0..8191 utiles ici, ton code la fait varier 0..32000 mais seuls 13 bits sont conservés)
bits [2:0] → id (3 bits, ici id = 3)


Découpe en 2 octets et envoie via com_send(peer_mac, data, 2).
Incrémente vitesse : vitesse = (vitesse + 1) % 32001.


Format message (2 octets)
MSB [ v12 v11 … v1 v0 | i2 i1 i0 ] LSB
où v = vitesse (13 bits), i = id (3 bits)

rx_task – Reçoit en bloquant

Appelle com_read_msg_wait(data, &len, &seq, portMAX_DELAY) (bloquant).
Recompose packed depuis data[0..1].
Dépaquette :

vitesse = (packed >> 3) & 0x1FFF
id = packed & 0x07


Toutes les 250 ms, affiche :
[RX] Dernier: ID=... | Vitesse=... | seq=... | total=...

seq : numéro de séquence fourni par la couche com (utile pour pertes/out-of-order)
total : nombre de paquets reçus depuis le démarrage



Couche com_* (abstraction ESP-NOW)

com_init(channel) : init ESP-NOW sur un canal Wi‑Fi donné.
com_get_mac(buf) : récupère la MAC locale.
com_add_peer(mac) : ajoute un pair (destinataire) à la table ESP-NOW.
com_send(mac, data, len) : envoie un message.
com_read_msg_wait(buf, &len, &seq, timeout) : lit un message (bloquant jusqu’au timeout ou réception).


⚠️ Assure-toi que les deux nœuds utilisent le même canal et que le peer est correctement ajouté côté émetteur.


🧪 Tester à 2 cartes

Carte A (TX+RX) : laisse le code tel quel, mets peer_mac = MAC de la Carte B.
Carte B (RX) : tu peux flasher le même firmware (il écoute aussi) et mettre peer_mac = MAC de la Carte A si tu veux tester en aller‑retour.


🔍 Dépannage

Rien reçu :

Vérifie canal identique sur les deux (channel).
Confirme la MAC du pair (peer_mac) et l’ordre des octets.
Assure-toi que le Wi‑Fi n’est pas déjà connecté à un AP sur un autre canal.


Beaucoup de pertes :

Réduis la fréquence (ex. période 5–10 ms).
Rapproche les cartes / évite les interférences (2.4 GHz).


Vitesse incohérente :

Rappelle-toi que seuls 13 bits sont transmis → plage utile 0..8191.
Si tu veux >8191, il faut envoyer plus d’octets.




📌 Paramètres clés

Taille message : 2 octets
Fréquence TX : ~500 Hz
Champs : vitesse (13 bits), id (3 bits)
Log RX : toutes 250 ms (configurable)