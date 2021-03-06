
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <inttypes.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <unistd.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <gtk/gtk.h>


#define MAXDATASIZE 256
#define VECTORNUMBER 8

/* Variables globales */
  int damier[8][8];	// tableau associe au damier [colone] [ligne]
  int couleur;		// 0 : pour noir, 1 : pour blanc
  
  int port;		// numero port passé lors de l'appel

  char *addr_j2, *port_j2;	// Info sur adversaire
  int vecteur_dir[VECTORNUMBER][2] ={{0,1},{1,1},{1,0},{1,-1},{0,-1},{-1,-1},{-1,0},{-1,1}}; //tout les vecteur possible pour retourner les pieces

  pthread_t thr_id;	// Id du thread fils gerant connexion socket
  
  int sock_fd, newsockfd=-1; // descripteurs de socket
  int addr_size;	 // taille adresse
  struct sockaddr *their_addr;
  struct addrinfo s_init, *p, *servinfo;	// structure pour stocker adresse adversaire

  fd_set master, read_fds, write_fds;	// ensembles de socket pour toutes les sockets actives avec select
  int fdmax;			// utilise pour select
  int couleurad;

  //nos variables
char msg[MAXDATASIZE];
char head[2];
uint16_t taille_msg;
int srvc_fd;
  typedef struct{
    int socket; 
  } threadArgs;

  //fin variables

//structure pour représenter un emplacement sur le plateau ou un vecteur direction
  typedef struct{
    int c;
    int l;
  } coor, vecteur; 

  
  // typedef struct{
  //   coor pion_ret;
  //   int param;
  //   int couleur;
  // } coorResult;

//permet de stocker une liste de piece à placer
  typedef struct{
    int nbret;
    coor* pion_ret;
  } result; 

/* Variables globales associées à l'interface graphique */
  GtkBuilder  *  p_builder   = NULL;
  GError      *  p_err       = NULL;
   

// nos fonctions

struct sockaddr * createSoc(int port) {
  struct sockaddr_in * sa =( struct sockaddr_in *) malloc(sizeof(struct sockaddr_in));

	sa->sin_family = AF_INET ;
	sa->sin_port = htons(port) ;
	sa->sin_addr.s_addr = INADDR_ANY ;
  return (struct sockaddr*)sa;
} 

struct send_position{
  uint16_t param;
  uint16_t x;
  uint16_t y;
  uint16_t couleur;
};
// fin de nos fonctions



// Entetes des fonctions  
  
/* Fonction permettant de changer l'image d'une case du damier (indiqué par sa colonne et sa ligne) */
void change_img_case(int col, int lig, int couleur_j);

/* Fonction permettant changer nom joueur blanc dans cadre Score */
void set_label_J1(char *texte);

/* Fonction permettant de changer nom joueur noir dans cadre Score */
void set_label_J2(char *texte);

/* Fonction permettant de changer score joueur blanc dans cadre Score */
void set_score_J1(int score);

/* Fonction permettant de récupérer score joueur blanc dans cadre Score */
int get_score_J1(void);

/* Fonction permettant de changer score joueur noir dans cadre Score */
void set_score_J2(int score);

/* Fonction permettant de récupérer score joueur noir dans cadre Score */
int get_score_J2(void);

/* Fonction transformant coordonnees du damier graphique en indexes pour matrice du damier */
void coord_to_indexes(const gchar *coord, int *col, int *lig);

/* Fonction appelee lors du clique sur une case du damier */
static void coup_joueur(GtkWidget *p_case);

/* Fonction retournant texte du champs adresse du serveur de l'interface graphique */
char *lecture_addr_serveur(void);

/* Fonction retournant texte du champs port du serveur de l'interface graphique */
char *lecture_port_serveur(void);

/* Fonction retournant texte du champs login de l'interface graphique */
char *lecture_login(void);

/* Fonction retournant texte du champs adresse du cadre Joueurs de l'interface graphique */
char *lecture_addr_adversaire(void);

/* Fonction retournant texte du champs port du cadre Joueurs de l'interface graphique */
char *lecture_port_adversaire(void);

/* Fonction affichant boite de dialogue si partie gagnee */
void affiche_fenetre_gagne(void);

/* Fonction affichant boite de dialogue si partie perdue */
void affiche_fenetre_perdu(void);

/* Fonction appelee lors du clique du bouton Se connecter */
static void clique_connect_serveur(GtkWidget *b);

/* Fonction desactivant bouton demarrer partie */
void disable_button_start(void);

/* Fonction appelee lors du clique du bouton Demarrer partie */
static void clique_connect_adversaire(GtkWidget *b);

/* Fonction desactivant les cases du damier */
void gele_damier(void);

/* Fonction activant les cases du damier */
void degele_damier(void);

/* Fonction permettant d'initialiser le plateau de jeu */
void init_interface_jeu(void);

/* Fonction reinitialisant la liste des joueurs sur l'interface graphique */
void reset_liste_joueurs(void);

/* Fonction permettant d'ajouter un joueur dans la liste des joueurs sur l'interface graphique */
void affich_joueur(char *login, char *adresse, char *port);

result* coup_valide(int col, int lig, int couleur);

void libere_result(result* r);

result* add_result(result* r,int c, int l);

void pose_piece(int c, int l,int couleur);

void change_couleur(result* r, int couleur);

int get_couleur_adversaire(void);

void send_position(result* r, int col, int lig);
//******* debut implémentation regles *****************///

//fonction de debug pour afficher ce qui est dans un resultat
void affiche_res(result* r)
{
  printf("retourne %d pieces\n", r->nbret);
  for (size_t i = 0; i < r->nbret; i++)
  {
    printf("piece retourné c: %d  l: %d\n", r->pion_ret[i].c, r->pion_ret[i].l);
  }
}

//renvoie vrai si la case demandé est hors du plateau
bool dans_plateau(int col, int lig){
  return !(col < 0 || col >= 8 || lig < 0 || lig >= 8);
}

//renvoie vrai si la case est 
bool case_vide(int col, int lig)
{
  return damier[col][lig] == -1;
}

//recupere la couleur de la piece ou - si la case est vide ou hors du plateau
int get_color(int col, int lig)
{
  if( (!dans_plateau(col, lig)) || case_vide(col, lig) )
    return -1;
  return damier[col][lig];
}


//ajouter une coordonee a la strucure result
result* add_result(result* r,int c, int l)
{
  r->nbret++;
  r->pion_ret = realloc(r->pion_ret , r->nbret * sizeof(coor));
  r->pion_ret[r->nbret -1].c = c;
  r->pion_ret[r->nbret -1].l = l;
  return r;
}

//free la liste de coordonee
void vide_list_coor(coor* c,int size)
{
  // for(int i = 0;i < size; i++)
  // {
  //   free(c+i);
  // }
  free(c);
}
//renvie un result videe de toute coordonee
result* vide_result(result* r)
{
  vide_list_coor(r->pion_ret, r->nbret);
  r->pion_ret=NULL;
  r->nbret = 0;
  return r;
}

//free la structure result
void libere_result(result* r)
{
  vide_list_coor(r->pion_ret,r->nbret);
  r->pion_ret=NULL;
  free(r);
}

//combine deux resultat ensemble
result* combine_result(result* r1, result* r2)
{
  r1->pion_ret = realloc(r1->pion_ret, (r1->nbret + r2->nbret) * sizeof(coor) );
  for (int i = 0; i < r2->nbret; i++)
  {
    r1->pion_ret[r1->nbret].c = r2->pion_ret[i].c;
    r1->pion_ret[r1->nbret].l = r2->pion_ret[i].l;
    r1->nbret++;
  }
  libere_result(r2);
  return r1;
}


//revoie un, result avec les pieces a retourner dans une direction donnee
result* retourne(int col, int lig, int couleur,vecteur v)
{

  result* r = (result *) malloc(sizeof(result));
  r->nbret=0;
  r->pion_ret=NULL;
  int loop_value;
  do 
  {
    col += v.c;
    lig += v.l;
    loop_value = get_color(col,lig);
    if(loop_value==-1){
      return vide_result(r);
    }
    if(loop_value == couleur){
      return r;
    }
    add_result(r, col,lig);
    

  }while (1);

}


//renvoie la liste de pice a retournee avec la piece nouvellement rajoute revoie un result vide si le placement n'est pas valide
result* coup_valide(int col, int lig, int couleur)
{
  if(!case_vide(col,lig)){
    result* r =malloc(sizeof(result));
    r->nbret=0;
    r->pion_ret=NULL;
    return r;
  }
  vecteur v;
  v.c=vecteur_dir[0][0];
  v.l=vecteur_dir[0][1];
  result* r1 = retourne(col,lig,couleur,v);
  for (int i =1; i < VECTORNUMBER;i++){
    v.c=vecteur_dir[i][0];
    v.l=vecteur_dir[i][1];
    r1=combine_result(r1,retourne( col,lig,couleur,v));
  }
  if(r1->nbret > 0)
    add_result(r1,col,lig);

  return r1;
}

//renvoie vrai si le joueur de la couleur passe en argument ne peu pas jouer
bool fin_de_partie(int couleur)
{
  result* r =malloc(sizeof(result));
  r->nbret=0;
  r->pion_ret=NULL;
  for(int i=0;i<8;i++)
  {
    for(int j=0;j<8;j++)
    {
      r = combine_result(r,coup_valide(i ,j ,couleur));
    }
  }
  return (r->nbret == 0);
}

//calcul le score de chacun des jouer et le met a jour
void change_score(){
  int j1 = 0;
  int j2 =0;
  for (int i=0;i<8;i++)
  {
    for (int j=0;j<8;j++){
      if(damier[i][j] == 0)
        j1++;
      if(damier[i][j] == 1)
        j2++;
      
    }
  }
  printf("Score J1 : %d\n", j1);
  printf("Score J2 : %d\n", j2);
  set_score_J1(j1);
  set_score_J2(j2);
}


int get_couleur_adversaire(){
  if (couleur == 1){
    return 0;
    //set_score_J2(2); //mise à jour score adversaire
  }else{
    return 1;
    //set_score_J1(2); //mise à jour score adversaire
  }
}
//******* fin implémentation regles *****************///


void pose_piece(int c, int l,int couleur)
{

    change_img_case(c, l, couleur);
    damier[c][l]= couleur;
}
void change_couleur(result* r, int couleur)
{
  for (size_t i = 0; i < r->nbret; i++)
  {
    pose_piece(r->pion_ret[i].c,r->pion_ret[i].l,couleur);
  }
}

void select_gagnant(){
  int j1 = get_score_J1();
  int j2 = get_score_J2();
  if(couleur == 1){
    int temps = j2;
    j2=j1;
    j1=temps;
  }
  if (j1 > j2){
      printf("affiche fenetre gagne\n");
      affiche_fenetre_gagne();
  }
  else{
    printf("affiche fenetre perdu\n");
    affiche_fenetre_perdu();
  }
}
/* Fonction transforme coordonnees du damier graphique en indexes pour matrice du damier */
void coord_to_indexes(const char *coord, int *col, int *lig)
{
  char *c;
  
  c=malloc(3*sizeof(char));
  
  c=strncpy(c, coord, 1);
  c[1]='\0';
  
  if(strcmp(c, "A")==0)
  {
    *col=0;
  }
  if(strcmp(c, "B")==0)
  {
    *col=1;
  }
  if(strcmp(c, "C")==0)
  {
    *col=2;
  }
  if(strcmp(c, "D")==0)
  {
    *col=3;
  }
  if(strcmp(c, "E")==0)
  {
    *col=4;
  }
  if(strcmp(c, "F")==0)
  {
    *col=5;
  }
  if(strcmp(c, "G")==0)
  {
    *col=6;
  }
  if(strcmp(c, "H")==0)
  {
    *col=7;
  }
    
  *lig=atoi(coord+1)-1;
}

/* Fonction transforme coordonnees du damier graphique en indexes pour matrice du damier */
void indexes_to_coord(int col, int lig, char *coord)
{
  char c;

  if(col==0)
  {
    c='A';
  }
  if(col==1)
  {
    c='B';
  }
  if(col==2)
  {
    c='C';
  }
  if(col==3)
  {
    c='D';
  }
  if(col==4)
  {
    c='E';
  }
  if(col==5)
  {
    c='F';
  }
  if(col==6)
  {
    c='G';
  }
  if(col==7)
  {
    c='H';
  }
    
  sprintf(coord, "%c%d\0", c, lig+1);
}

/* Fonction permettant de changer l'image d'une case du damier (indiqué par sa colonne et sa ligne) */
void change_img_case(int col, int lig, int couleur_j)
{
  char * coord;
  
  coord=malloc(3*sizeof(char));

  indexes_to_coord(col, lig, coord);
  
  if(couleur_j)
  { // image pion blanc
    gtk_image_set_from_file(GTK_IMAGE(gtk_builder_get_object(p_builder, coord)), "UI_Glade/case_blanc.png");
  }
  else
  { // image pion noir
    gtk_image_set_from_file(GTK_IMAGE(gtk_builder_get_object(p_builder, coord)), "UI_Glade/case_noir.png");
  }
}

/* Fonction permettant changer nom joueur blanc dans cadre Score */
void set_label_J1(char *texte)
{
  gtk_label_set_text(GTK_LABEL(gtk_builder_get_object (p_builder, "label_J1")), texte);
}

/* Fonction permettant de changer nom joueur noir dans cadre Score */
void set_label_J2(char *texte)
{
  gtk_label_set_text(GTK_LABEL(gtk_builder_get_object (p_builder, "label_J2")), texte);
}

/* Fonction permettant de changer score joueur blanc dans cadre Score */
void set_score_J1(int score)
{
  char *s;
  
  s=malloc(5*sizeof(char));
  sprintf(s, "%d\0", score);
  
  gtk_label_set_text(GTK_LABEL(gtk_builder_get_object (p_builder, "label_ScoreJ1")), s);
}

/* Fonction permettant de récupérer score joueur blanc dans cadre Score */
int get_score_J1(void)
{
  const gchar *c;
  
  c=gtk_label_get_text(GTK_LABEL(gtk_builder_get_object (p_builder, "label_ScoreJ1")));
  
  return atoi(c);
}

/* Fonction permettant de changer score joueur noir dans cadre Score */
void set_score_J2(int score)
{
  char *s;
  
  s=malloc(5*sizeof(char));
  sprintf(s, "%d\0", score);
  
  gtk_label_set_text(GTK_LABEL(gtk_builder_get_object (p_builder, "label_ScoreJ2")), s);
}

/* Fonction permettant de récupérer score joueur noir dans cadre Score */
int get_score_J2(void)
{
  const gchar *c;
  
  c=gtk_label_get_text(GTK_LABEL(gtk_builder_get_object (p_builder, "label_ScoreJ2")));
  
  return atoi(c);
}


/* Fonction appelee lors du clique sur une case du damier */
static void coup_joueur(GtkWidget *p_case)
{
  int col, lig, type_msg, nb_piece, score;

  // Traduction coordonnees damier en indexes matrice damier
  coord_to_indexes(gtk_buildable_get_name(GTK_BUILDABLE(gtk_bin_get_child(GTK_BIN(p_case)))), &col, &lig);

  /***** TO DO info dernier coup joué et regle, coup valide*****/
    
  result *res = coup_valide(col, lig, couleur);
  if (res->nbret !=0 ){
    send_position(res, col, lig);
  }else if (fin_de_partie(couleur)){
    if (fin_de_partie(get_couleur_adversaire())){
      printf("Fin de partie");
      select_gagnant();
    }
    else{
      printf("Ne peux pas jouer\n");
      send_position(res, col, lig);
      }
    }
}

/* Fonction changeant la couleur des pions de notre plateau et envoie à l'adversaire notre pion joué */
void send_position(result *res, int col, int lig){
  gele_damier();
  if (res->nbret !=0 ){
    change_couleur(res, couleur);
  } 
  
  printf("----------- New position send\n");
  change_score();
  bzero(msg, MAXDATASIZE);
  snprintf(msg, MAXDATASIZE, "%u, %u", lig, col);
  libere_result(res);
  if (send(newsockfd, &msg, strlen(msg), 0) == -1){
    perror("send");
    close(newsockfd);
  }
}
/* Fonction retournant texte du champs adresse du serveur de l'interface graphique */
char *lecture_addr_serveur(void)
{
  GtkWidget *entry_addr_srv;
  
  entry_addr_srv = (GtkWidget *) gtk_builder_get_object(p_builder, "entry_adr");
  
  return (char *)gtk_entry_get_text(GTK_ENTRY(entry_addr_srv));
}

/* Fonction retournant texte du champs port du serveur de l'interface graphique */
char *lecture_port_serveur(void)
{
  GtkWidget *entry_port_srv;
  
  entry_port_srv = (GtkWidget *) gtk_builder_get_object(p_builder, "entry_port");
  
  return (char *)gtk_entry_get_text(GTK_ENTRY(entry_port_srv));
}

/* Fonction retournant texte du champs login de l'interface graphique */
char *lecture_login(void)
{
  GtkWidget *entry_login;
  
  entry_login = (GtkWidget *) gtk_builder_get_object(p_builder, "entry_login");
  
  return (char *)gtk_entry_get_text(GTK_ENTRY(entry_login));
}

/* Fonction retournant texte du champs adresse du cadre Joueurs de l'interface graphique */
char *lecture_addr_adversaire(void)
{
  GtkWidget *entry_addr_j2;
  
  entry_addr_j2 = (GtkWidget *) gtk_builder_get_object(p_builder, "entry_addr_j2");
  
  return (char *)gtk_entry_get_text(GTK_ENTRY(entry_addr_j2));
}

/* Fonction retournant texte du champs port du cadre Joueurs de l'interface graphique */
char *lecture_port_adversaire(void)
{
  GtkWidget *entry_port_j2;
  
  entry_port_j2 = (GtkWidget *) gtk_builder_get_object(p_builder, "entry_port_j2");
  
  return (char *)gtk_entry_get_text(GTK_ENTRY(entry_port_j2));
}

/* Fonction affichant boite de dialogue si partie gagnee */
void affiche_fenetre_gagne(void)
{
  GtkWidget *dialog;
    
  GtkDialogFlags flags = GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;
  
  dialog = gtk_message_dialog_new(GTK_WINDOW(gtk_builder_get_object(p_builder, "window1")), flags, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, "Fin de la partie.\n\n Vous avez gagné!!!");
  gtk_dialog_run(GTK_DIALOG (dialog));
  
  gtk_widget_destroy(dialog);
}

/* Fonction affichant boite de dialogue si partie perdue */
void affiche_fenetre_perdu(void)
{
  GtkWidget *dialog;
    
  GtkDialogFlags flags = GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;
  
  dialog = gtk_message_dialog_new(GTK_WINDOW(gtk_builder_get_object(p_builder, "window1")), flags, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, "Fin de la partie.\n\n Vous avez perdu!");
  gtk_dialog_run(GTK_DIALOG (dialog));
  
  gtk_widget_destroy(dialog);
}

/* Fonction appelee lors du clique du bouton Se connecter */
static void clique_connect_serveur(GtkWidget *b)
{

  /***** TO DO *****/
  
  // struct sockaddr_in * sa =( struct sockaddr_in *) malloc(sizeof(struct sockaddr_in));
  

	// sa->sin_family = AF_INET;

	// sa->sin_port = htons(lecture_port_serveur());

	// sa->sin_addr.s_addr = (long)inet_addr(lecture_addr_serveur());

	// int sd = socket(AF_INET, SOCK_STREAM, 0);

	// int error = connect(sd, (struct sockaddr*) &sa, sizeof(sa));
  // printf("%d", error);
  // fflush(stdout);
	// if (error == -1) {
	// 	return error;
	// } else {
  //   affich_joueur(lecture_login(), lecture_addr_serveur(),lecture_port_serveur());
	// 	return sd;
	// }
}

/* Fonction desactivant bouton demarrer partie */
void disable_button_start(void)
{
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object (p_builder, "button_start"), FALSE);
}

/* Fonction traitement signal bouton Demarrer partie */
static void clique_connect_adversaire(GtkWidget *b)
{
  if(newsockfd==-1)
  {
    // Deactivation bouton demarrer partie
    gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object (p_builder, "button_start"), FALSE);
    
    // Recuperation  adresse et port adversaire au format chaines caracteres
    addr_j2=lecture_addr_adversaire();
    port_j2=lecture_port_adversaire();
    
    printf("[Port joueur : %d] Adresse j2 lue : %s\n",port, addr_j2);
    printf("[Port joueur : %d] Port j2 lu : %s\n", port, port_j2);

    
    pthread_kill(thr_id, SIGUSR1); 
  }
}

/* Fonction desactivant les cases du damier */
void gele_damier(void)
{
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxA1"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxB1"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxC1"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxD1"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxE1"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxF1"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxG1"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxH1"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxA2"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxB2"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxC2"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxD2"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxE2"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxF2"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxG2"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxH2"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxA3"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxB3"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxC3"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxD3"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxE3"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxF3"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxG3"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxH3"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxA4"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxB4"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxC4"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxD4"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxE4"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxF4"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxG4"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxH4"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxA5"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxB5"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxC5"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxD5"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxE5"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxF5"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxG5"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxH5"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxA6"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxB6"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxC6"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxD6"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxE6"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxF6"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxG6"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxH6"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxA7"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxB7"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxC7"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxD7"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxE7"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxF7"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxG7"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxH7"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxA8"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxB8"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxC8"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxD8"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxE8"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxF8"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxG8"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxH8"), FALSE);
}

/* Fonction activant les cases du damier */
void degele_damier(void)
{
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxA1"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxB1"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxC1"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxD1"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxE1"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxF1"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxG1"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxH1"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxA2"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxB2"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxC2"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxD2"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxE2"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxF2"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxG2"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxH2"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxA3"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxB3"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxC3"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxD3"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxE3"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxF3"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxG3"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxH3"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxA4"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxB4"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxC4"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxD4"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxE4"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxF4"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxG4"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxH4"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxA5"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxB5"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxC5"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxD5"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxE5"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxF5"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxG5"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxH5"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxA6"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxB6"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxC6"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxD6"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxE6"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxF6"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxG6"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxH6"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxA7"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxB7"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxC7"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxD7"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxE7"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxF7"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxG7"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxH7"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxA8"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxB8"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxC8"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxD8"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxE8"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxF8"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxG8"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "eventboxH8"), TRUE);
}

/* Fonction permettant d'initialiser le plateau de jeu */
void init_interface_jeu(void)
{
  // Initilisation du damier (D4=blanc, E4=noir, D5=noir, E5=blanc)
  // change_img_case(3, 3, 1);
  // change_img_case(4, 3, 0);
  // change_img_case(3, 4, 0);
  // change_img_case(4, 4, 1);
  
  // Initialisation des scores et des joueurs
  if(couleur==1)
  {
    set_label_J1("Vous");
    set_label_J2("Adversaire");
    
  }
  else
  {
    set_label_J1("Adversaire");
    set_label_J2("Vous");
    gele_damier();
  }

  set_score_J1(2);
  set_score_J2(2);
  
  /***** TO DO *****/

  
}

/* Fonction reinitialisant la liste des joueurs sur l'interface graphique */
void reset_liste_joueurs(void)
{
  GtkTextIter start, end;
  
  gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtk_builder_get_object(p_builder, "textview_joueurs")))), &start);
  gtk_text_buffer_get_end_iter(GTK_TEXT_BUFFER(gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtk_builder_get_object(p_builder, "textview_joueurs")))), &end);
  
  gtk_text_buffer_delete(GTK_TEXT_BUFFER(gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtk_builder_get_object(p_builder, "textview_joueurs")))), &start, &end);
}

/* Fonction permettant d'ajouter un joueur dans la liste des joueurs sur l'interface graphique */
void affich_joueur(char *login, char *adresse, char *port)
{
  const gchar *joueur;
  
  joueur=g_strconcat(login, " - ", adresse, " : ", port, "\n", NULL);
  
  gtk_text_buffer_insert_at_cursor(GTK_TEXT_BUFFER(gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtk_builder_get_object(p_builder, "textview_joueurs")))), joueur, strlen(joueur));
}

/* Fonction exécutée par le thread gérant les communications à travers la socket */
static void * f_com_socket(void *p_arg)
{
  fflush(stdout);
  int i, nbytes, col, lig;
  
  char buf[MAXDATASIZE], *tmp, *p_parse;
  int len, bytes_sent, t_msg_recu;

  sigset_t signal_mask;
  int fd_signal;
  
  uint16_t type_msg, col_j2;
  uint16_t ucol, ulig;

  char *token;
  char *saveptr;
  /* Association descripteur au signal SIGUSR1 */
  sigemptyset(&signal_mask);
  fflush(stdout);
  sigaddset(&signal_mask, SIGUSR1);
  fflush(stdout);
    
  if(sigprocmask(SIG_BLOCK, &signal_mask, NULL) == -1)
  {
    printf("[Port joueur %d] Erreur sigprocmask\n", port);
    
    return 0;
  }
  fflush(stdout);
  fd_signal = signalfd(-1, &signal_mask, 0);
    
  if(fd_signal == -1)
  {
    printf("[port joueur %d] Erreur signalfd\n", port);

    return 0;
  }
  fflush(stdout);
  /* Ajout descripteur du signal dans ensemble de descripteur utilisé avec fonction select */
  FD_SET(fd_signal, &master);
  
  if(fd_signal>fdmax)
  {
    fdmax=fd_signal;
  }

  fflush(stdout);
  while(1)
  {
    read_fds=master;	// copie des ensembles
    
    if(select(fdmax+1, &read_fds, &write_fds, NULL, NULL)==-1)
    {
      perror("Problème avec select");
      exit(4);
    }
    
    printf("[Port joueur %d] Entree dans boucle for\n", port);
    for(i=0; i<=fdmax; i++)
    {
      //printf("[Port joueur %d] newsockfd=%d, iteration %d boucle for\n", port, newsockfd, i);
      //printf("toto, %i\n", FD_ISSET(i, &read_fds));
      if(FD_ISSET(i, &read_fds))
      {
        printf("ISSET\n") ;
        if(i==fd_signal)
        {
          /* Cas où de l'envoie du signal par l'interface graphique pour connexion au joueur adverse */
          /***** TO DO *****/
          printf("connection\n");
          if (newsockfd == -1){
            int rv;
            memset(&s_init, 0, sizeof(s_init));

            s_init.ai_family = AF_UNSPEC;
            s_init.ai_socktype = SOCK_STREAM;
            rv = getaddrinfo(lecture_addr_adversaire(), lecture_port_adversaire(), &s_init, &servinfo);
            if(rv != 0) 
            {
              fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
              exit(1);
            }
            for(p = servinfo; p != NULL; p = p->ai_next) 
            {
              if((newsockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
                perror("client: socket");
                continue;
              }
              printf("newsockfd : %d\n", newsockfd);
              if((connect(newsockfd, p->ai_addr, p->ai_addrlen)) == -1) {
                close(newsockfd);
                perror("client: connect");
                continue;
              }
              break;
            }

              freeaddrinfo(servinfo);
              printf("Connection is OK\n");
              FD_SET(newsockfd, &master);
              if (newsockfd > fdmax){
                  fdmax = newsockfd; 
              }
              close(sock_fd);
              FD_CLR(sock_fd, &master);

              close(fd_signal);
              FD_CLR(fd_signal, &master);

              //res->param = -1;
              couleur = rand()%2;
              bzero(msg, MAXDATASIZE);
              snprintf(msg, MAXDATASIZE, "%u", get_couleur_adversaire());
              if(send(newsockfd, &msg, strlen(msg), 0) == -1){
               perror("send");
              }
              printf("Init interface\n");
              init_interface_jeu();
          }
        }
        if(i==sock_fd)
        { // Acceptation connexion adversaire
          printf("Acceptation connexion adversaire\n");
          /***** TO DO *****/
	        if (newsockfd == -1){
              
              addr_size = sizeof(their_addr);
              newsockfd = accept(sock_fd, (struct sockaddr *) &their_addr, &addr_size);
              if (newsockfd == -1) {
                perror("accept");
              }else{
                FD_SET(newsockfd, &master);
                if (newsockfd > fdmax){
                  fdmax = newsockfd;
                }
                FD_CLR(sock_fd, &master);
                close(sock_fd);
              }
              printf("Serveur: acceptation d'un joueur\n");
              
              close(fd_signal);
              FD_CLR(fd_signal, &master);

              bzero(msg, MAXDATASIZE);
              recv(newsockfd, msg, MAXDATASIZE, 0);
              printf("reception d'un msg \n");
              

              token = strtok_r(msg, ",", &saveptr);
              sscanf(token, "%u", &couleur);
 

              printf("Message reçu %u\n", couleur);
              
              printf("Init interface\n");
              init_interface_jeu();

              gtk_widget_set_sensitive((GtkWidget *)
                          gtk_builder_get_object
                          (p_builder, "button_start"),
                          FALSE);
              
          }
        
         
        }
        else
        { // Reception et traitement des messages du joueur adverse
            /***** TO DO *****/
            if (i == newsockfd){
              
              printf("----------- Reception\n");

              coor* coord = (coor*) malloc(sizeof(coor));
              int col, lig;
              
              int nb = recv(newsockfd, msg, MAXDATASIZE, 0);
              
              token = strtok_r(msg, ",", &saveptr);
              sscanf(token, "%u", &lig);

              token = strtok_r(NULL, ",", &saveptr);
              sscanf(token, "%u", &col);              

          
              printf(" Joueur %d joue col : %d et lig : %d\n", get_couleur_adversaire(), col, lig);
              result *res = coup_valide(col, lig, get_couleur_adversaire());

              if (res->nbret !=0 ){
                change_couleur(res, get_couleur_adversaire());
                affiche_res(res);
              } 

              change_score();
               if (fin_de_partie(couleur)){
                  if (fin_de_partie(get_couleur_adversaire())){
                    select_gagnant();
                  }
                }
              libere_result(res);
              printf("Degele damier\n");
              degele_damier();
            }

        }
      }
    }
  }
  
  return NULL;
}


int main (int argc, char ** argv)
{

   int i, j;

   if(argc!=2)
   {
     printf("\nPrototype : ./othello num_port\n\n");
     
     exit(1);
   }
   
   
   /* Initialisation de GTK+ */
   gtk_init (& argc, & argv);
   
   /* Creation d'un nouveau GtkBuilder */
   p_builder = gtk_builder_new();
 
   if (p_builder != NULL)
   {
      /* Chargement du XML dans p_builder */
      gtk_builder_add_from_file (p_builder, "UI_Glade/Othello.glade", & p_err);
 
      if (p_err == NULL)
      {
         /* Recuparation d'un pointeur sur la fenetre. */
         GtkWidget * p_win = (GtkWidget *) gtk_builder_get_object (p_builder, "window1");

 
         /* Gestion evenement clic pour chacune des cases du damier */
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxA1"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxB1"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxC1"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxD1"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxE1"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxF1"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxG1"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxH1"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxA2"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxB2"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxC2"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxD2"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxE2"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxF2"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxG2"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxH2"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxA3"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxB3"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxC3"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxD3"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxE3"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxF3"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxG3"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxH3"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxA4"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxB4"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxC4"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxD4"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxE4"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxF4"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxG4"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxH4"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxA5"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxB5"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxC5"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxD5"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxE5"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxF5"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxG5"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxH5"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxA6"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxB6"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxC6"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxD6"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxE6"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxF6"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxG6"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxH6"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxA7"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxB7"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxC7"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxD7"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxE7"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxF7"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxG7"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxH7"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxA8"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxB8"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxC8"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxD8"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxE8"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxF8"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxG8"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "eventboxH8"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
         
         /* Gestion clic boutons interface */
         g_signal_connect(gtk_builder_get_object(p_builder, "button_connect"), "clicked", G_CALLBACK(clique_connect_serveur), NULL);
         g_signal_connect(gtk_builder_get_object(p_builder, "button_start"), "clicked", G_CALLBACK(clique_connect_adversaire), NULL);
         
         /* Gestion clic bouton fermeture fenetre */
         g_signal_connect_swapped(G_OBJECT(p_win), "destroy", G_CALLBACK(gtk_main_quit), NULL);
         
         
         
         /* Recuperation numero port donne en parametre */
         port=atoi(argv[1]);
          
         /* Initialisation du damier de jeu */
         for(i=0; i<8; i++)
         {
           for(j=0; j<8; j++)
           {
             damier[i][j]=-1; 
           }  
         }

         
        pose_piece(3,3,1);
        pose_piece(4,3,0);
        pose_piece(4,4,1);
        pose_piece(3,4,0);


         /***** TO DO *****/
         
         // Initialisation socket et autres objets, et création thread pour communications avec joueur adverse
         //new -------------------------------------
        
        //struct sockaddr * sa= createSoc(port);

        memset(&s_init, 0, sizeof(s_init));
        s_init.ai_family = AF_UNSPEC;
        s_init.ai_socktype = SOCK_STREAM;
        s_init.ai_flags = AI_PASSIVE;

        char sport[2];
        sprintf(sport, "%d", port);
        if (getaddrinfo(NULL, sport, &s_init, &servinfo) != 0){
          fprintf(stderr, "Erreur getaddrinfo\n");
          exit(1);
        }

        for(p = servinfo; p != NULL; p = p -> ai_next){

          if ((sock_fd = socket( p->ai_family, p->ai_socktype,  p->ai_protocol)) == -1) {
            perror("Serveur: socket");
            continue;
          }

          if (bind(sock_fd,p->ai_addr, p->ai_addrlen) == -1) {
            close(sock_fd);
            perror("Serveur: erreur bind");
            continue;
          }
          break;
        }


        if (p == NULL) {
          fprintf(stderr, "Serveur: echec bind\n");
         exit(2);
        }
          
        freeaddrinfo(servinfo);

        if (listen(sock_fd, 1) == -1) {
          perror("listen");
          exit(1);
        }

        FD_ZERO(&master);  // clear the master and temp sets
        FD_ZERO(&read_fds);


        FD_SET(sock_fd, &master); // Ajout sockfd à ensemble
      
        fdmax=sock_fd; 
        read_fds = master;  // Garde valeur max socket
	      pthread_create(&thr_id, NULL, f_com_socket, NULL);
        gtk_widget_show_all(p_win);
        gtk_main();

       
      }
      else
      {
         /* Affichage du message d'erreur de GTK+ */
         g_error ("%s", p_err->message);
         g_error_free (p_err);
      }
   }

   
   // end -------------------------------------------------------
 
 
   return EXIT_SUCCESS;
}

