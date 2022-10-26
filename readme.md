# OS Project : Clock

Ci-dessous le descriptif des différentes fonctionnalités du projet Clock et ses particularités.

## Interface : ncurses

Tout d'abord, l'interface de ce programme a été réalisée avec la librairie **ncurses** qui permet de manipuler une interface en ligne de commande d'un terminal afin de la rendre plus "ludique". 
Le programme se lance donc **depuis un terminal** via la commande **``./clock``**.  Ensuite, le réveil s'utilise sans ligne de commande, mais **uniquement à l'aide de votre clavier**, comme expliqué dans les différentes fonctionnalités ci-après.


## Fonctionnalités


Clock est un réveil pour environnement linux et possède plusieurs fonctionnalités :

- La première est l'affichage de **l'heure.**
	> L'heure est affichée **"par défaut"**. C'est-à-dire qu'elle s'affiche en haut lors du lancement du programme et reste **en arrière plan** de manière **permanante** jusqu'a l'extinction du programme (qui s'effectue en appuyant sur **q**) : 
	**``./clock``**
	
- La deuxième est le **Chronomètre**. C'est par définition un chronomètre qui va compter à l'infini et qui peut etre mis sur **pause** et/ou **enregistrer trois temps distincts**.
	> Il s'utilise en appuyant sur la touche **c** ou **e** de votre clavier. Il apparaitra dans un nouveau terminal.
	**La mise en pause** et la reprise se font à l'aide de la touche **espace**.
	**L'enregistrement des trois temps** se font respectivement à l'aide des touches **w**, **x** et **c**.
	Pour **quitter** ce mode, il suffit d'appuyer sur **q**, suivi de la touche correspondant à la fonctionnalité suivante souhaitée, si besoin est.

- La troisième est le **Timer**. C'est un compte à rebours depuis une *input_value* jusqu'à *zéro*.
	> Il s'utilise en appuyant sur la touche **t** ou **z** de votre clavier. Un **curseur** apparait alors en-dessous de l'heure (à la place du chronomètre) dans lequel votre **input_value** doit être renseignée dans le même format que pour l'alarme expliqué ci-après.
	Il se lancera alors dans un nouveau terminal.
	La touche **q** permet également de quitter ce mode. 
	

- La quatrième est **l'alarme**. Par définition, vous serez avertis lorsque l'heure atteindra votre *input_time* entrée.
	> Il s'utilise en appuyant sur la touche **a** de votre clavier. Un **curseur** apparait alors en-dessous de l'heure dans lequel votre ***input_time*** sera renseignée sous  le format **..h..m..s** (préciser les minutes et les secondes est facultatif mais cela sera considéré comme 00min et/ou 00sec si non renseigné) :
	Une fois votre heure enregistrée, **elle sera indiquée en petit, juste en dessous de l'heure actuelle et tournera en arrière plan**. Vous serez alors averti lorsque l'heure atteindra celle voulue, et pourrez alors profiter des autres fonctionnalités en attendant. *Vous connaissez les règles et nous aussi...*
	
- La dernière est les **statistiques**. Quelques informations intéressantes sur vos actions effectuées seront affichées, également dans un nouveau terminal.
	> Elle s'utilise en appuyant sur la touche **s** ou **r** de votre clavier et **q** pour en sortir. 


Pour compiler, il faut utiliser la commande **``gcc main.c -o clock -lncursesw``**
