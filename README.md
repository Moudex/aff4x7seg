# aff4x7seg
Driver Linux pour 4 afficheurs 7 segments.
## Implantation du périphérique
![Schéma d'implantation général du périphérique](./aff4x7seg_schema.png)
Branchez les afficheurs comme indiqué sur le schéma ci-dessus. Les numéros de broches peuvent être modifiés dans les _defines_ au début du code source. Les diodes L2 et L2 représentent ":" entre _Digit 2_ et _Digit 3_. La diode L3 représente "°" entre _Digit 3_ et _Digit 4_.

Basé sur le LTC-4727.
## Installation
```
$ make
# mknod /dev/aff4x7seg c 240 0
# insmod aff4x7seg.ko
```
Ajustez les permissions sur le fichier _/dev/aff4x7seg_ pour permettre à d'autres utilisateur d'afficher un message.
## Utilisation
Les fonctions _open()_, _read()_, _write()_ et _close()_ sont autorisés.

Par exemple pour afficher un message:
```
# echo "-18°C" > /dev/aff4x7seg
```
Pour lire le message:
```
# cat /dev/aff4x7seg
```
