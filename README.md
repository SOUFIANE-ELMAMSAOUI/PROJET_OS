# Système d'exploitation ARM Cortex-M33
## Description
- Ce projet implémente un système d'exploitation multitâche simple pour processeur ARM Cortex-M33, développé dans le cadre d'un travail pratique sur les fondements des systèmes d'exploitation embarqués.
  
## Objectifs pédagogiques

- Comprendre les mécanismes fondamentaux d'un OS multitâche
- Implémenter la gestion des tâches et la commutation de contexte
- Maîtriser les appels système et la synchronisation par sémaphores
- Développer une couche d'abstraction pour l'accès aux périphériques

## Architecture du système
### Organisation générale
#### Le système est organisé en deux parties principales :

- Code applicatif : s'exécute en mode Thread unprivileged
- Système d'exploitation : composé du noyau (mode Handler privileged) et d'une couche driver

## Fonctionnalités implémentées
###  Noyau

- Appels système (SVC) : interface entre code utilisateur et noyau
- Gestion des tâches : création, commutation, terminaison
- Ordonnanceur : algorithme Round Robin avec interruption SysTick
- Sémaphores : synchronisation et exclusion mutuelle
- Temporisation : endormissement des tâches avec task_wait()

### 💾 Gestion mémoire

- Allocation dynamique (os_alloc, os_free)
- Piles dédiées par tâche avec protection

###  Périphériques

- Couche VFS : interface standardisée (open, read, write, close)
- Périphérique de test : lecture de chaîne statique
- LEDs RGB : contrôle des LEDs de la carte
- Bouton utilisateur : gestion d'interruption
