#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <inttypes.h>

#include <tox/tox.h>
#include "md5.h"
#include "server_by_tox_protocol_file_transfer.h"

#define UNUSED_VAR(x) ((void) x)


/*******************************************************************************
 *
 * Consts & Macros
 *
 ******************************************************************************/

// where to save the tox data.
// if don't want to save, set it to NULL.
const char *savedata_filename = "./savedata.tox";
const char *savedata_tmp_filename = "./savedata.tox.tmp";

struct DHT_node {
    const char *ip;
    uint16_t port;
    const char key_hex[TOX_PUBLIC_KEY_SIZE*2 + 1];
};

struct DHT_node bootstrap_nodes[] = {

    // Setup tox bootrap nodes
    
        {"91.219.59.156",                          33445, "8E7D0B859922EF569298B4D261A8CCB5FEA14FB91ED412A7603A585A25698832"},
        {"tox.initramfs.io",           33445, "3F0A45A268367C1BEA652F258C85F4A66DA76BCAA667A49E770BCC4917AB6A25"},
        {"2a03:b0c0:3:d0::ac:5001",     33445, "CD133B521159541FB1D326DE9850F5E56A6C724B5B8E5EB5CD8D950408E95707"},

};

#define TRUNCATE_SIZE 1000000 // 1MB
#define LINE_MAX_SIZE 5120  // If input line's length surpassed this value, it will be truncated.
#define FILENAME_MAX_SIZE 256
#define STREAM_MAX_SIZE 256
#define TIME_FORMAT_MAX_SIZE 128
#define PORT_RANGE_START 33445     // tox listen port range
#define PORT_RANGE_END   34445
#define ID_MAX_SIZE 76
#define AREPL_INTERVAL  30  // Async REPL iterate interval. unit: millisecond.

#define DEFAULT_CHAT_HIST_COUNT  20 // how many items of chat history to show by default;

#define SAVEDATA_AFTER_COMMAND true // whether save data after executing any command

/// Macros for terminal display


struct ServerStatus {
	uint8_t status; //0: ok; 1: receving file; 2: sending file
	struct FileTransfer *curtransfile;
	uint64_t timeelapsed; //milli seconds
};

static uint64_t stt_glb_mtimeforcmd=0;
static int stt_glb_ts=0;
static int stt_glb_msgid;
static char stt_glb_str_cmdid[16];
static uint64_t stt_glb_foffset=0;
static int stt_glb_hash=0;
static int stt_glb_hashprocessid=-1;
static int stt_glb_countforhash=0;
static int stt_glb_hasclient=0;
static struct ServerStatus *stt_glb_ptr_sStatus;

static char stt_glb_str_clientid[ID_MAX_SIZE]="A93FC6C1352AAFF908DD09E79D87BE024FFFBE7A49007F6A276F045C4B438171D7E74C6C62F7";
//static char stt_glb_str_fname[FILENAME_MAX_SIZE];
static char stt_glb_str_curpathtofile[LINE_MAX_SIZE];
//static char stt_glb_str_fpath[LINE_MAX_SIZE];
//static char stt_glb_str_mtimeclient[TIME_FORMAT_MAX_SIZE];
//static const char stt_glb_str_pathlogfile[]="/var/res/share/.synctempdir/synservertoxlog.txt";
//static const char stt_glb_str_maindir[]="/var/res/share/syncdir";
//static const char stt_glb_str_tempdir[]="/var/res/share/.synctempdir";
static const char stt_glb_str_pathlogfile[]="/var/res/backup/.Temp/mainlog.txt";
static const char stt_glb_str_maindir[]="/var/res/backup/SyncDir";
static const char stt_glb_str_tempdir[]="/var/res/backup/.Temp";
//static const char stt_glb_str_lsfile[]="lsfile.txt";
static char stt_glb_str_hexname[] = "synserverbytoxprotocolDungntUbuntu";
static const char stt_glb_str_tempfile[] = "tempfile";
static const char stt_glb_str_resultfile[] = "resultfile";
static char stt_glb_str_fullresultfile[LINE_MAX_SIZE];
static char stt_glb_str_fullpathtofile[LINE_MAX_SIZE];

static struct Friend *friendTakingto;

//for message id
//0: seto; 1: wrst; 2:mdir; 3:setu; 4:prup

/*******************************************************************************
 *
 * Headers
 *
 ******************************************************************************/

Tox *tox;


struct FriendUserData {
    uint8_t pubkey[TOX_PUBLIC_KEY_SIZE];
};

union RequestUserData {

    struct FriendUserData friend;
};

struct Request {
    char *msg;
    uint32_t id;
    bool is_friend_request;
    union RequestUserData userdata;
    struct Request *next;
};

/*
struct ChatHist {
    char *msg;
    struct ChatHist *next;
    struct ChatHist *prev;
};
*/

struct Request *requests = NULL;

struct Friend *friends = NULL;
struct Friend self;


enum TALK_TYPE { TALK_TYPE_FRIEND, TALK_TYPE_COUNT, TALK_TYPE_NULL = UINT32_MAX };

uint32_t TalkingTo = TALK_TYPE_NULL;

/*******************************************************************************
 *
 * Declare Functions
 *
 ******************************************************************************/

void write_to_log_file(char *msg);
void start_send_file(Tox *m, uint32_t friendnum, char *pathtofile, char *msgout, int c);//need for friend_message_cb
//void auto_accept(int narg, char *args, bool is_accept) ;
void update_savedata_file(void);
void on_file_recv_cb(Tox *tox, uint32_t friendnumber, uint32_t filenumber, uint32_t kind, uint64_t file_size,
                  const uint8_t *filename, size_t filename_length, void *userdata);

void on_file_recv_chunk_cb(Tox *m, uint32_t friendnumber, uint32_t filenumber, uint64_t position,
                        const uint8_t *data, size_t length, void *userdata);

void on_file_recv_control_cb(Tox *m, uint32_t friendnumber, uint32_t filenumber, Tox_File_Control control,
                          void *userdata);

void on_file_chunk_request_cb(Tox *m, uint32_t friendnumber, uint32_t filenumber, uint64_t position,
                           size_t length, void *userdata);
/*******************************************************************************
 *
 * Common Functions
 *
 ******************************************************************************/

void show_err_and_stop_program(char *msg){
	write_to_log_file(msg);
	printf("Error! %s",msg);
	//end program
	exit(0);
}

char *get_Last_Directory(const char *path)
{
    char *pend;

    if ((pend = strrchr(path, '/')) != NULL)
        return pend + 1;

    return NULL;
}

size_t get_Pos_Of_Last_Directory(const char *path)
{
    char *pend;

    if ((pend = strrchr(path, '/')) != NULL)
        return pend - path;

    return 0;
}

bool str2uint(char *str, uint32_t *num) {
    char *str_end;
    long l = strtol(str,&str_end,10);
    if (str_end == str || l < 0 ) return false;
    *num = (uint32_t)l;
    return true;
}

/*
char* genmsg(struct ChatHist **pp, const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);

    va_list va2;
    va_copy(va2, va);
    size_t len = vsnprintf(NULL, 0, fmt, va2);
    va_end(va2);

    struct ChatHist *h = malloc(sizeof(struct ChatHist));
    h->prev = NULL;
    h->next = (*pp);
    if (*pp) (*pp)->prev = h;
    *pp = h;
    h->msg = malloc(len+1);

    vsnprintf(h->msg, len+1, fmt, va);
    va_end(va);

    return h->msg;
}
*/

const char * connection_enum2text(TOX_CONNECTION conn) {
    switch (conn) {
        case TOX_CONNECTION_NONE:
            return "Of";
        case TOX_CONNECTION_TCP:
            return "OnT";
        case TOX_CONNECTION_UDP:
            return "OnU";
        default:
            return "sno";
    }
}

/* Converts bytes to appropriate unit and puts in buf as a string */
void bytes_convert_str(char *buf, int size, uint64_t bytes)
{
    double conv = bytes;
    const char *unit;

    if (conv < KiB) {
        unit = "Bytes";
    } else if (conv < MiB) {
        unit = "KiB";
        conv /= (double) KiB;
    } else if (conv < GiB) {
        unit = "MiB";
        conv /= (double) MiB;
    } else {
        unit = "GiB";
        conv /= (double) GiB;
    }

    snprintf(buf, size, "%.1f %s", conv, unit);
}

static bool valid_file_name(const char *filename, size_t length)
{
    if (length == 0) {
        return false;
    }

    if (filename[0] == ' ' || filename[0] == '-') {
        return false;
    }

    if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0) {
        return false;
    }

    for (size_t i = 0; i < length; ++i) {
        if (filename[i] == '/') {
            return false;
        }
    }

    return true;
}

/* gets base file name from path or original file name if no path is supplied.
 * Returns the file name length
 */
size_t get_file_name(char *namebuf, size_t bufsize, const char *pathname)
{
    int len = strlen(pathname) - 1;
    char *path = strdup(pathname);

    //if (path == NULL) {
    //    exit_toxic_err("failed in get_file_name", FATALERR_MEMORY);
    //}

    while (len >= 0 && pathname[len] == '/') {
        path[len--] = '\0';
    }

    char *finalname = strdup(path);

    //if (finalname == NULL) {
    //    exit_toxic_err("failed in get_file_name", FATALERR_MEMORY);
    //}

    const char *basenm = strrchr(path, '/');

    if (basenm != NULL) {
        if (basenm[1]) {
            strcpy(finalname, &basenm[1]);
        }
    }

    snprintf(namebuf, bufsize, "%s", finalname);
    free(finalname);
    free(path);

    return strlen(namebuf);
}

/* returns file size. If file doesn't exist returns 0. */
off_t file_size(const char *path)
{
    struct stat st;

    if (stat(path, &st) == -1) {
        return 0;
    }

    return st.st_size;
}

struct Friend *getfriend(uint32_t friend_num) {
    struct Friend **p = &friends;
    LIST_FIND(p, (*p)->friend_num == friend_num);
    return *p;
}

struct Friend *addfriend(uint32_t friend_num) {
    struct Friend *f = calloc(1, sizeof(struct Friend));
    f->next = friends;
    friends = f;
    f->friend_num = friend_num;
    f->connection = TOX_CONNECTION_NONE;
    tox_friend_get_public_key(tox, friend_num, f->pubkey, NULL);
    return f;
}


uint8_t *hex2bin(const char *hex)
{
    size_t len = strlen(hex) / 2;
    uint8_t *bin = malloc(len);

    for (size_t i = 0; i < len; ++i, hex += 2) {
        sscanf(hex, "%2hhx", &bin[i]);
    }

    return bin;
}

char *bin2hex(const uint8_t *bin, size_t length) {
    char *hex = malloc(2*length + 1);
    char *saved = hex;
    for (int i=0; i<length;i++,hex+=2) {
        sprintf(hex, "%02X",bin[i]);
    }
    return saved;
}



int string2int(char *str){
	int x;
	x = (int)strtol(str, NULL, 10);
    //printf("Converting '122': %d\n", x);
	return x;
}


void auto_add_noreq(char *id) {
    char *hex_id = id;
    //char *msg = "";

    uint8_t *bin_id = hex2bin(hex_id);
    TOX_ERR_FRIEND_ADD err;
    uint32_t friend_num = tox_friend_add_norequest(tox, bin_id, &err);
    free(bin_id);

    if (err != TOX_ERR_FRIEND_ADD_OK) {
        printf("! add friend failed, errcode:%d\n",err);
        return;
    }

    addfriend(friend_num);
    update_savedata_file();
}


bool delfriend(uint32_t friend_num) {
    struct Friend **p = &friends;
    LIST_FIND(p, (*p)->friend_num == friend_num);
    struct Friend *f = *p;
    if (f) {
        *p = f->next;
        if (f->name) free(f->name);
        if (f->status_message) free(f->status_message);
        //while (f->hist) {
        //    struct ChatHist *tmp = f->hist;
        //    f->hist = f->hist->next;
        //    free(tmp);
        //}
        free(f);
        return 1;
    }
    return 0;
}

/*
void delallfriends(){
    struct Friend **p = &friends;    
    struct Friend *f = NULL;
    uint32_t num = 0;
    while(*p != NULL){
        f = *p;
        *p = f->next;
        num = f->friend_num;
        if (f->name) free(f->name);
        if (f->status_message) free(f->status_message);
        free(f);       
        tox_friend_delete(tox, num, NULL);
    }

    update_savedata_file();    
}
*/

/*******************************************************************************
 *
 * Server Side CMD
 *
 ******************************************************************************/
 int run_command(char *s) {
 	char cmd[LINE_MAX_SIZE];
 	FILE * stream;
 	char buffer[STREAM_MAX_SIZE];
 	size_t n=0;

    snprintf(cmd, LINE_MAX_SIZE, "rm %s/%s", stt_glb_str_tempdir, stt_glb_str_resultfile);

    stream = popen(cmd, "r");
 	if (stream) pclose(stream);
 	
	snprintf(cmd, LINE_MAX_SIZE, "%s  > %s/%s 2>&1; echo $?;", s, stt_glb_str_tempdir, stt_glb_str_resultfile);

 	printf("runcmd:%s\n",cmd);
    buffer[0]='\0';
 	stream = popen(cmd, "r");
 	if (stream) {
 		while (!feof(stream)){
 			if (fgets(buffer, STREAM_MAX_SIZE, stream) != NULL){
 				n = strlen(buffer);
 			}
 		}
 		pclose(stream);
 	}

 	if(n>0){
      int i = strtol(buffer, NULL, 10);
      return i;   
    } 
 	else return -1;
}
 
void check_dir(int c, char *out) {
	char cmd[LINE_MAX_SIZE];

	FILE * stream;
	char buffer[STREAM_MAX_SIZE];
	size_t n=0,m=0;


	if(c==0)
		snprintf(cmd, LINE_MAX_SIZE, "if [ ! -d %s ] ; then echo 1; else echo 0; fi",stt_glb_str_maindir);
	else
		snprintf(cmd, LINE_MAX_SIZE, "if [ ! -d %s ] ; then echo 1; else echo 0; fi",stt_glb_str_tempdir);


    buffer[0]='\0';
	stream = popen(cmd, "r");
	if (stream) {
		while (!feof(stream)){
			if (fgets(buffer, STREAM_MAX_SIZE, stream) != NULL){
				n = strlen(buffer);
				memcpy(out+m,buffer,n);
				m+=n;
			}
		}
		pclose(stream);
	}

	out[m]='\0';
}

int run_hash() {
	char cmd[LINE_MAX_SIZE];

	FILE * stream;
	char buffer[STREAM_MAX_SIZE];
	size_t n=0;

    snprintf(cmd, 4*LINE_MAX_SIZE, "if [ -f %s ] ; then md5sum %s > %s & echo $!; else echo 0; fi", stt_glb_str_fullpathtofile, stt_glb_str_fullpathtofile, stt_glb_str_fullresultfile);
	printf("cmd:%s\n",cmd);

    buffer[0]='\0';
	stream = popen(cmd, "r");
	if (stream) {
		while (!feof(stream)){
			if (fgets(buffer, STREAM_MAX_SIZE, stream) != NULL){
				n = strlen(buffer);
			}
		}
		pclose(stream);
	}

    if(n>0){
        int i = strtol(buffer, NULL, 10);
        printf("kqchaycmd:%d\n",i);
        return i;
    }

    return -1;
}

int kill_hash_process() {
	char cmd[LINE_MAX_SIZE];

	FILE * stream;
	char buffer[STREAM_MAX_SIZE];
	size_t n=0;

    snprintf(cmd, LINE_MAX_SIZE, "kill %d; echo $?;", stt_glb_hashprocessid);
	printf("cmd:%s\n",cmd);

    buffer[0]='\0';
	stream = popen(cmd, "r");
	if (stream) {
		while (!feof(stream)){
			if (fgets(buffer, STREAM_MAX_SIZE, stream) != NULL){
				n = strlen(buffer);
			}
		}
		pclose(stream);
	}

    if(n>0){
        int i = strtol(buffer, NULL, 10);        
        return i;
    }

    return -1;
}

int move_file() {
	char cmd[2*LINE_MAX_SIZE];

	FILE * stream;
	char buffer[STREAM_MAX_SIZE];
	size_t n=0;
	printf("mv to '%s'\n",stt_glb_str_fullpathtofile);
    snprintf(cmd, 2*LINE_MAX_SIZE, "mv %s/tempfile '%s'; echo $?;",stt_glb_str_tempdir, stt_glb_str_fullpathtofile);	

    buffer[0]='\0';
	stream = popen(cmd, "r");
	if (stream) {
		while (!feof(stream)){
			if (fgets(buffer, STREAM_MAX_SIZE, stream) != NULL){
				n = strlen(buffer);
			}
		}
		pclose(stream);
	}
	
	printf("%d kq cau lenh %s\n",n,buffer);
	if(n>0 && buffer[0]=='0') return 1;
    //else if(n>0 && buffer[0]=='1') return 0;
    else {
    	printf("mv to '%s'\n",stt_glb_str_fullpathtofile);
		snprintf(cmd, 2*LINE_MAX_SIZE, "mv %s/tempfile \"%s\"; echo $?;",stt_glb_str_tempdir, stt_glb_str_fullpathtofile);	

		buffer[0]='\0';
		stream = popen(cmd, "r");
		if (stream) {
			while (!feof(stream)){
				if (fgets(buffer, STREAM_MAX_SIZE, stream) != NULL){
					n = strlen(buffer);
				}
			}
			pclose(stream);
		}
		
		printf("%d kq cau lenh %s\n",n,buffer);
		if(n>0 && buffer[0]=='0') return 1;
		else return 0;
    }
}


int truncate_get_size_tempfile(uint64_t curmtimecmd){
	char tempfilepath[LINE_MAX_SIZE];
	char cmd[LINE_MAX_SIZE];
	FILE * stream;
	char buffer[STREAM_MAX_SIZE];
	size_t n=0;

	snprintf(tempfilepath,LINE_MAX_SIZE,"%s/%s", stt_glb_str_tempdir,stt_glb_str_tempfile);

	off_t fs = file_size(tempfilepath);

	int ts = fs/TRUNCATE_SIZE;

    ts--;

    if(stt_glb_mtimeforcmd == curmtimecmd){
        if(ts < stt_glb_ts) ts = stt_glb_ts;
    }
    else{
        stt_glb_mtimeforcmd = curmtimecmd;
        if(ts < 0) ts = 0;
        stt_glb_ts = ts;
    }

	snprintf(cmd, LINE_MAX_SIZE, "if [ ! -f %s/%s ] ; then touch %s/%s; echo $?; else truncate --size=%dMB %s/%s; echo $?; fi", stt_glb_str_tempdir, stt_glb_str_tempfile, stt_glb_str_tempdir, stt_glb_str_tempfile, ts, stt_glb_str_tempdir, stt_glb_str_tempfile);

	printf("truncatecmd:%s\n",cmd);

	buffer[0]='\0';
	stream = popen(cmd, "r");
	if (stream) {
		while (!feof(stream)){
			if (fgets(buffer, STREAM_MAX_SIZE, stream) != NULL){
				n = strlen(buffer);
			}
		}
		pclose(stream);
	}

	if((n>0)&&(buffer[0]=='0')) return ts;
	else return -1;
}

/*******************************************************************************
 *
 * Client-Respond Functions
 *
 ******************************************************************************/
 
 void respond_msg(uint32_t friend_num, const uint8_t *message){
    int i=run_command((char*)message);
	char out[STREAM_MAX_SIZE];
	
	snprintf(out,STREAM_MAX_SIZE,"%d 1 5 %d",stt_glb_msgid,i);
	printf("out:%s\n",out);
	tox_friend_send_message(tox, friend_num, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *)out, strlen(out), NULL);
 }

void respond_hash(uint32_t friend_num, const uint8_t *message){
	char out[LINE_MAX_SIZE];
	
    int i,j=1;
    for(i=0;i<strlen((char*)message);i++)
        if((message[i]=='/')&&(message[i+1]=='/')) {
            if(message[i+2]=='.'){
                j=0;                
            }
            break;
        }

    if(j==0)
        snprintf(stt_glb_str_fullpathtofile,LINE_MAX_SIZE,"%s%s",stt_glb_str_tempdir,(char*)message+i+5);
    else
        snprintf(stt_glb_str_fullpathtofile,LINE_MAX_SIZE,"%s%s",stt_glb_str_maindir,(char*)message+i+5);

    char *p = (char*)message;
    p[i] = '\0';

    if(stt_glb_hash == 0){           
        snprintf(stt_glb_str_cmdid, 16, "%s", (char*)message);
        printf("idcmd:%s---filenametohash:%s\n",stt_glb_str_cmdid,stt_glb_str_fullpathtofile);
        remove(stt_glb_str_fullresultfile);
        int k = run_hash();

        if(k > 0){
            stt_glb_hash = 1;
            stt_glb_hashprocessid = k;
            stt_glb_countforhash = 0;
            snprintf(out,LINE_MAX_SIZE,"%d 1",-99);
            printf("out:%s\n",out);            
            tox_friend_send_message(tox, friend_num, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *)out, strlen(out), NULL);
        }
        else {
            stt_glb_hash = 0;
            stt_glb_hashprocessid = -1;
            stt_glb_countforhash = 0;
            snprintf(out,STREAM_MAX_SIZE,"%d 1 7 0",stt_glb_msgid);
            printf("out:%s\n",out);
            tox_friend_send_message(tox, friend_num, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *)out, strlen(out), NULL);
        }
    }
    else {
        if(strcmp(stt_glb_str_cmdid,(char*)message)!=0) {//new hash command
            kill_hash_process();

            snprintf(stt_glb_str_cmdid, 16, "%s", (char*)message);
            printf("idcmd:%s---filenametohash:%s\n",stt_glb_str_cmdid,stt_glb_str_fullpathtofile);
            remove(stt_glb_str_fullresultfile);
            int k = run_hash();

            if(k > 0){                
                stt_glb_hash = 1;
                stt_glb_hashprocessid = k;
                stt_glb_countforhash = 0;
                snprintf(out,LINE_MAX_SIZE,"%d 1",-99);
                printf("out:%s\n",out);            
                tox_friend_send_message(tox, friend_num, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *)out, strlen(out), NULL);
            }
            else {
                stt_glb_hash = 0;
                stt_glb_hashprocessid = -1;
                stt_glb_countforhash = 0;
                snprintf(out,STREAM_MAX_SIZE,"%d 1 7 0",stt_glb_msgid);
                printf("out:%s\n",out);
                tox_friend_send_message(tox, friend_num, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *)out, strlen(out), NULL);
            }
        }
    }
 }


void respond_send_result_file(uint32_t friend_num, const uint8_t *message){
	char out[LINE_MAX_SIZE];
	
	snprintf(out,LINE_MAX_SIZE,"%d 1",stt_glb_msgid);
	printf("out:%s\n",out);
    stt_glb_foffset = 0;
    start_send_file(tox, friend_num, NULL, out, 0);
 }

void respond_send_file(uint32_t friend_num, const uint8_t *message){
	char out[LINE_MAX_SIZE];
	
	snprintf(out,LINE_MAX_SIZE,"%d 1",stt_glb_msgid);
	printf("out:%s\n",out);

    int i;
    for(i=0;i<strlen((char*)message);i++)
        if((message[i]=='/')) break;

    char n[16];
    memcpy(n,(char*)message,i);
    n[i] = '\0';

    int ts = strtol(n,NULL,10);
    printf("truncsizefromclient:%d\n",ts);

    stt_glb_foffset = ts*TRUNCATE_SIZE;

    int j=1;
    for(i=0;i<strlen((char*)message);i++)
        if((message[i]=='/')&&(message[i+1]=='/')) {
            if(message[i+2]=='.'){
                j=0;                
            }
            break;
        }

    printf("%dfilenametosendtoclient:%s\n",j,(char*)message+i+5);

    if(j==0){
        stt_glb_foffset = 0;
        start_send_file(tox, friend_num, (char*)message+i+5, out, 2);
    }
    else
        start_send_file(tox, friend_num, (char*)message+i+5, out, 1);
 }

 void respond_up_file(uint32_t friend_num, const uint8_t *message){
	char out[LINE_MAX_SIZE];
		
    int i,j=1;
    for(i=0;i<strlen((char*)message);i++)
        if((message[i]=='/')&&(message[i+1]=='/')) {
            if(message[i+2]=='.'){
                j=0;                
            }
            break;
        }

    char str_mtimecmd[17];
    memcpy(str_mtimecmd,message, i);
    str_mtimecmd[i] = '\0';
    uint64_t mtimecmd = (uint64_t)strtol(str_mtimecmd,NULL,10);
    printf("mtimecmd:%lld\n",mtimecmd);

    if(j==1){
        int ts = truncate_get_size_tempfile(mtimecmd);

        snprintf(out,LINE_MAX_SIZE,"%d 1 6 %d",stt_glb_msgid,ts);
        printf("out:%s\n",out);

        snprintf(stt_glb_str_fullpathtofile,LINE_MAX_SIZE,"%s%s",stt_glb_str_maindir,message + i + 5);
    } else {
        snprintf(out,LINE_MAX_SIZE,"%d 1 6 0",stt_glb_msgid);
        printf("out:%s\n",out);

        snprintf(stt_glb_str_fullpathtofile,LINE_MAX_SIZE,"%s%s",stt_glb_str_tempdir,message + i + 5);
    }

    printf("filetosave:%s\n",stt_glb_str_fullpathtofile);
    tox_friend_send_message(tox, friend_num, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *)out, strlen(out), NULL);
 }

/*******************************************************************************
 *
 * Tox Callbacks
 *
 ******************************************************************************/


void friend_message_cb(Tox *tox, uint32_t friend_num, TOX_MESSAGE_TYPE type, const uint8_t *message,
                                   size_t length, void *user_data)
{
    struct Friend *f = getfriend(friend_num);
    if (!f) return;
    if (type != TOX_MESSAGE_TYPE_NORMAL) {
        printf("\n****************** receive MESSAGE ACTION type from %s, no supported\n", f->name);
        return;
    }

    printf("\n****************** receive MESSAGE - %s - from %s\n", message, f->name);

    char stt_glb_msgid_inchar[4];
    memcpy(stt_glb_msgid_inchar,(char*)message,3);
    stt_glb_msgid_inchar[3] = '\0';

    stt_glb_msgid = (int)strtol(stt_glb_msgid_inchar,NULL,10);
    printf("msg_id=%d\n", stt_glb_msgid);

    
    char s[5];
    memcpy(s, (char*)message+4,4);
    s[4]='\0';

    if(strcmp(s,"hash")==0){
        respond_hash(friend_num,message+8);
    }
    else { 
        if(stt_glb_hash == 1){
            kill_hash_process();
            stt_glb_hash = 0;
            stt_glb_hashprocessid = -1;
            stt_glb_countforhash = 0;
        }

        if(strcmp(s,"cmd/")==0){
            respond_msg(friend_num, message+8);
        }
        else if(strcmp(s,"dowf")==0){
            respond_send_result_file(friend_num,message+8);
        }       
        else if(strcmp(s,"down")==0){
            respond_send_file(friend_num,message+8);
        } 		
        else if(strcmp(s,"upfl")==0){
            respond_up_file(friend_num,message+8);
        }         
    }
}

void friend_request_cb(Tox *tox, const uint8_t *public_key, const uint8_t *message, size_t length, void *user_data) {
    //do nothing
}

void friend_name_cb(Tox *tox, uint32_t friend_num, const uint8_t *name, size_t length, void *user_data) {
	struct Friend *f = getfriend(friend_num);

    if (f) {
        f->name = realloc(f->name, length+1);
        sprintf(f->name, "%.*s", (int)length, (char*)name);
        if (GEN_INDEX(friend_num, TALK_TYPE_FRIEND) == TalkingTo) {
            printf("* Opposite changed name to %.*s", (int)length, (char*)name);
            //sprintf(async_repl->prompt, FRIEND_TALK_PROMPT, f->name);
        }
    }
}

void friend_status_message_cb(Tox *tox, uint32_t friend_num, const uint8_t *message, size_t length, void *user_data) {
    struct Friend *f = getfriend(friend_num);
    if (f) {
        f->status_message = realloc(f->status_message, length + 1);
        sprintf(f->status_message, "%.*s",(int)length, (char*)message);
    }
}

void friend_connection_status_cb(Tox *tox, uint32_t friend_num, TOX_CONNECTION connection_status, void *user_data)
{
    struct Friend *f = getfriend(friend_num);
    if (f) {
        f->connection = connection_status;

       char buffer[256];
       snprintf(buffer, sizeof(buffer), "%s",connection_enum2text(connection_status));
       printf("* %s is %s", f->name, buffer);
    }
}

/*
void friend_request_cb(Tox *tox, const uint8_t *public_key, const uint8_t *message, size_t length, void *user_data) {
    printf("* receive friend request(use `/accept` to see).");

    struct Request *req = malloc(sizeof(struct Request));

    req->id = 1 + ((requests != NULL) ? requests->id : 0);
    req->is_friend_request = true;
    memcpy(req->userdata.friend.pubkey, public_key, TOX_PUBLIC_KEY_SIZE);
    req->msg = malloc(length + 1);
    sprintf(req->msg, "%.*s", (int)length, (char*)message);
    req->next = requests;
    requests = req;

   if (strcmp((char*)message, "autotox") == 0){
	   printf("* xin lam ban ok, tu dong autoaccept\n");
	   auto_accept(1,"1",true);
	   update_savedata_file();
   }
   else{
	   printf("* !xin lam ban not ok\n");
   }
}
*/

void self_connection_status_cb(Tox *tox, TOX_CONNECTION connection_status, void *user_data)
{
    self.connection = connection_status;
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s",connection_enum2text(connection_status));
    printf("* You are %s\n", buffer);
}



/*******************************************************************************
 *
 * Tox Setup
 *
 ******************************************************************************/

void create_tox(void)
{
    struct Tox_Options *options = tox_options_new(NULL);
    tox_options_set_start_port(options, PORT_RANGE_START);
    tox_options_set_end_port(options, PORT_RANGE_END);

    if (savedata_filename) {
        FILE *f = fopen(savedata_filename, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long fsize = ftell(f);
            fseek(f, 0, SEEK_SET);

            char *savedata = malloc(fsize);
            fread(savedata, fsize, 1, f);
            fclose(f);

            tox_options_set_savedata_type(options, TOX_SAVEDATA_TYPE_TOX_SAVE);
            tox_options_set_savedata_data(options, (uint8_t*)savedata, fsize);

            tox = tox_new(options, NULL);

            free(savedata);
        }
    }

    if (!tox) tox = tox_new(options, NULL);
    tox_options_free(options);
}

void init_friends(void) {
    size_t sz = tox_self_get_friend_list_size(tox);
    uint32_t *friend_list = malloc(sizeof(uint32_t) * sz);
    tox_self_get_friend_list(tox, friend_list);

    size_t len;

    for (int i = 0;i<sz;i++) {
        uint32_t friend_num = friend_list[i];
        struct Friend *f = addfriend(friend_num);

        len = tox_friend_get_name_size(tox, friend_num, NULL) + 1;
        f->name = calloc(1, len);
        tox_friend_get_name(tox, friend_num, (uint8_t*)f->name, NULL);

        len = tox_friend_get_status_message_size(tox, friend_num, NULL) + 1;
        f->status_message = calloc(1, len);
        tox_friend_get_status_message(tox, friend_num, (uint8_t*)f->status_message, NULL);

        tox_friend_get_public_key(tox, friend_num, f->pubkey, NULL);
    }
    free(friend_list);

    // add self
    self.friend_num = TALK_TYPE_NULL;
    len = tox_self_get_name_size(tox) + 1;
    self.name = calloc(1, len);
    tox_self_get_name(tox, (uint8_t*)self.name);

    len = tox_self_get_status_message_size(tox) + 1;
    self.status_message = calloc(1, len);
    tox_self_get_status_message(tox, (uint8_t*)self.status_message);

    tox_self_get_public_key(tox, self.pubkey);
}

void update_savedata_file(void)
{
    if (!(savedata_filename && savedata_tmp_filename)) return;

    size_t size = tox_get_savedata_size(tox);
    char *savedata = malloc(size);
    tox_get_savedata(tox, (uint8_t*)savedata);

    FILE *f = fopen(savedata_tmp_filename, "wb");
    fwrite(savedata, size, 1, f);
    fclose(f);

    rename(savedata_tmp_filename, savedata_filename);

    free(savedata);
}

void bootstrap(void)
{
    for (size_t i = 0; i < sizeof(bootstrap_nodes)/sizeof(struct DHT_node); i ++) {
        uint8_t *bin = hex2bin(bootstrap_nodes[i].key_hex);
        tox_bootstrap(tox, bootstrap_nodes[i].ip, bootstrap_nodes[i].port, bin, NULL);
        free(bin);
    }
}

void setup_tox(void)
{
    create_tox();
    init_friends();
    bootstrap();

    ////// register callbacks

    // self
    tox_callback_self_connection_status(tox, self_connection_status_cb);

    // friend
    tox_callback_friend_request(tox, friend_request_cb);//this for friend auto accept
    tox_callback_friend_message(tox, friend_message_cb);
    tox_callback_friend_name(tox, friend_name_cb);
    tox_callback_friend_status_message(tox, friend_status_message_cb);
    tox_callback_friend_connection_status(tox, friend_connection_status_cb);

    //savefile
    tox_callback_file_recv(tox, on_file_recv_cb);
    tox_callback_file_chunk_request(tox, on_file_chunk_request_cb);
    tox_callback_file_recv_control(tox, on_file_recv_control_cb);
    tox_callback_file_recv_chunk(tox, on_file_recv_chunk_cb);
}

/*******************************************************************************
 *
 * Commands
 *
 ******************************************************************************/



void _print_friend_info(struct Friend *f, bool is_self) {
    printf("%-15s%s\n", "Name:", f->name);

    if (is_self) {
        uint8_t tox_id_bin[TOX_ADDRESS_SIZE];
        tox_self_get_address(tox, tox_id_bin);
        char *hex = bin2hex(tox_id_bin, sizeof(tox_id_bin));
        printf("%-15s%s\n","Tox ID:", hex);
        free(hex);
    }

    char *hex = bin2hex(f->pubkey, sizeof(f->pubkey));
    printf("%-15s%s\n","Public Key:", hex);
    free(hex);
    printf("%-15s%s\n", "Status Msg:",f->status_message);
    printf("%-15s%s\n", "Network:",connection_enum2text(f->connection));
}



int auto_del(char *args) {
    uint32_t contact_idx;
    if (!str2uint(args, &contact_idx)) goto FAIL;
    uint32_t num = INDEX_TO_NUM(contact_idx);
    switch (INDEX_TO_TYPE(contact_idx)) {
        case TALK_TYPE_FRIEND:
			if (delfriend(num)) {
				tox_friend_delete(tox, num, NULL);
				update_savedata_file();
				return 1;
			} else return -1;

            break;

    }
FAIL:
    printf("^ Invalid contact index\n");
    return -1;
}

/*

void auto_accept(int narg, char *args, bool is_accept) {
	printf("in auto_accept narg=%d %s\n",narg,args);
    if (narg == 0) {
        struct Request * req = requests;
        for (;req != NULL;req=req->next) {
            printf("%-9u%-12s%s\n", req->id, (req->is_friend_request ? "FRIEND" : "NOT SUPPORTED"), req->msg);
        }
        return;
    }

    uint32_t request_idx;
    if (!str2uint(args, &request_idx)) goto FAIL;
    struct Request **p = &requests;
    LIST_FIND(p, (*p)->id == request_idx);
    struct Request *req = *p;
    if (req) {
        *p = req->next;
        if (is_accept) {
            if (req->is_friend_request) {
                TOX_ERR_FRIEND_ADD err;
                uint32_t friend_num = tox_friend_add_norequest(tox, req->userdata.friend.pubkey, &err);
                if (err != TOX_ERR_FRIEND_ADD_OK) {
                    printf("! accept friend request failed, errcode:%d", err);
                } else {
                    addfriend(friend_num);
                }
            }
        }
        free(req->msg);
        free(req);
        return;
    }
FAIL:
    printf("Invalid request index");
}

*/

/*******************************************************************************
 *
 * ControlFile
 *
 ******************************************************************************/

 static void onFileControl(Tox *m, uint32_t friendnum, uint32_t filenumber,
                               Tox_File_Control control)
{

    struct Friend *f = getfriend(friendnum);

    struct FileTransfer *ft = get_file_transfer_struct(f, filenumber);


    switch (control) {
        case TOX_FILE_CONTROL_RESUME: {
            /* transfer is accepted */
            if (ft->state == FILE_TRANSFER_PENDING) {
                ft->state = FILE_TRANSFER_STARTED;
                /*line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "File transfer [%zu] for '%s' accepted.",
                              ft->index, ft->file_name);
                char progline[MAX_STR_SIZE];
                init_progress_bar(progline);
                line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "%s", progline);
                sound_notify(self, silent, NT_NOFOCUS | user_settings->bell_on_filetrans_accept | NT_WNDALERT_2, NULL);
                ft->line_id = self->chatwin->hst->line_end->id + 2;*/
            } else if (ft->state == FILE_TRANSFER_PAUSED) {    // transfer is resumed
                ft->state = FILE_TRANSFER_STARTED;
            }

            break;
        }

        case TOX_FILE_CONTROL_PAUSE: {
            ft->state = FILE_TRANSFER_PAUSED;
            break;
        }

        case TOX_FILE_CONTROL_CANCEL: {
            char msg[MAX_STR_SIZE];
            snprintf(msg, sizeof(msg), "File transfer for '%s' was aborted.", ft->file_name);
            close_file_transfer( m, ft, -1, msg);

            break;
        }
    }
}

 void on_file_recv_control_cb(Tox *m, uint32_t friendnumber, uint32_t filenumber, Tox_File_Control control,
                          void *userdata){
		  onFileControl(m, friendnumber, filenumber, control);
}

/*******************************************************************************
 *
 * SendFile
 *
 ******************************************************************************/

void start_send_file(Tox *m, uint32_t friendnum, char *pathtofile, char *msgout, int c) //tuong dong cmd_sendfile o toxic
{
    struct Friend *f = getfriend(friendnum);

    char path[MAX_STR_SIZE];
    
    if(c==0)
        snprintf(path, sizeof(path), "%s/%s", stt_glb_str_tempdir, stt_glb_str_resultfile);
    else if(c==1)
        snprintf(path, sizeof(path), "%s%s", stt_glb_str_maindir, pathtofile);
    else
        snprintf(path, sizeof(path), "%s%s", stt_glb_str_tempdir, pathtofile);

    /*
    int path_len = strlen(path);

    if (path_len >= MAX_STR_SIZE) {
        tox_friend_send_message(tox, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *)msgout, strlen(msgout), NULL);
        return;
    }*/

    FILE *file_to_send = fopen(path, "r");

    if (file_to_send == NULL) {
        tox_friend_send_message(tox, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *)msgout, strlen(msgout), NULL);    
        return;
    }

    off_t filesize = file_size(path) - (off_t)stt_glb_foffset;

    if (filesize == 0) {
        char strmsg[16];
        if(c==0)
            snprintf(strmsg,16,"%s %d",msgout,8);//if sent result_file
        else
            snprintf(strmsg,16,"%s %d",msgout,9);//if sent other

        tox_friend_send_message(tox, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *)strmsg, strlen(strmsg), NULL);
        fclose(file_to_send);
        return;
    }

    printf("filetosend:%s\n",path);
    char file_name[TOX_MAX_FILENAME_LENGTH];
    size_t namelen = get_file_name(file_name, sizeof(file_name), path);

    Tox_Err_File_Send err;

    uint32_t filenum = tox_file_send(m, friendnum, TOX_FILE_KIND_DATA, (uint64_t) filesize, NULL, (uint8_t *) file_name, namelen, &err);

    if (err != TOX_ERR_FILE_SEND_OK) {
        goto on_send_error;
    }

    struct FileTransfer *ft = new_file_transfer(f,friendnum, filenum, FILE_TRANSFER_SEND, TOX_FILE_KIND_DATA);

    if (!ft) {
        err = TOX_ERR_FILE_SEND_TOO_MANY;
        goto on_send_error;
    }

    memcpy(ft->file_name, file_name, namelen + 1);
    ft->file = file_to_send;
    ft->file_size = filesize;
    tox_file_get_file_id(m, friendnum, filenum, ft->file_id, NULL);

	ft->position = (uint64_t)stt_glb_foffset;
	if (fseek(ft->file, ft->position, SEEK_SET) == -1) {
		goto on_send_error;
	}


    printf("Sending file [%d]: '%s' \n", filenum, file_name);

    stt_glb_ptr_sStatus->curtransfile = ft;
	stt_glb_ptr_sStatus->status=2;
	stt_glb_ptr_sStatus->timeelapsed=0;

    return;

on_send_error:

	stt_glb_ptr_sStatus->status=0;

    switch (err) {
        case TOX_ERR_FILE_SEND_FRIEND_NOT_FOUND:
            //errmsg = "File transfer failed: Invalid friend.";
            break;

        case TOX_ERR_FILE_SEND_FRIEND_NOT_CONNECTED:
            //errmsg = "File transfer failed: Friend is offline.";
            break;

        case TOX_ERR_FILE_SEND_NAME_TOO_LONG:
            //errmsg = "File transfer failed: Filename is too long.";
            break;

        case TOX_ERR_FILE_SEND_TOO_MANY:
            //errmsg = "File transfer failed: Too many concurrent file transfers.";
            break;

        default:
            //errmsg = "File transfer failed.";
            break;
    }

    tox_file_control(m, friendnum, filenum, TOX_FILE_CONTROL_CANCEL, NULL);
    fclose(file_to_send);
}

static void onFileChunkRequest(Tox *m, uint32_t friendnum, uint32_t filenumber, uint64_t position, size_t length)
{

    struct Friend *f = getfriend(friendnum);

    struct FileTransfer *ft = get_file_transfer_struct(f, filenumber);

    if (!ft) {
		printf("File NULL\n");
        return;
    }

    if (ft->state != FILE_TRANSFER_STARTED) {
		printf("File state wrong\n");
        return;
    }

    stt_glb_ptr_sStatus->timeelapsed=50;
	uint64_t kp = position;
    char msg[MAX_STR_SIZE];

    if (length == 0) {
        snprintf(msg, sizeof(msg), "File '%s' successfully sent.", ft->file_name);
        close_file_transfer(m, ft, -1, msg);
        stt_glb_ptr_sStatus->status=0;
        return;
    }

    if (ft->file == NULL) {
        snprintf(msg, sizeof(msg), "File transfer for '%s' failed: Null file pointer.", ft->file_name);
        close_file_transfer(m, ft, TOX_FILE_CONTROL_CANCEL, msg);
        stt_glb_ptr_sStatus->status=0;
        return;
    }

	kp+=(uint64_t)stt_glb_foffset;

	//printf("%"PRIu64"\n", position);
	//printf("%"PRIu64"\n", ft->position);

    if (ft->position != kp) {
        if (fseek(ft->file, kp, SEEK_SET) == -1) {
            snprintf(msg, sizeof(msg), "File transfer for '%s' failed: Seek fail.", ft->file_name);
            close_file_transfer(m, ft, TOX_FILE_CONTROL_CANCEL, msg);
            stt_glb_ptr_sStatus->status=0;
            return;
        }
		printf("seek file ok\n");
        ft->position = kp;
    }

    uint8_t *send_data = malloc(length);

    if (send_data == NULL) {
        snprintf(msg, sizeof(msg), "File transfer for '%s' failed: Out of memory.", ft->file_name);
        close_file_transfer(m, ft, TOX_FILE_CONTROL_CANCEL, msg);
        stt_glb_ptr_sStatus->status=0;
        return;
    }

    size_t send_length = fread(send_data, 1, length, ft->file);

    if (send_length != length) {
        snprintf(msg, sizeof(msg), "File transfer for '%s' failed: Read fail.", ft->file_name);
        close_file_transfer(m, ft, TOX_FILE_CONTROL_CANCEL, msg);
        free(send_data);
        stt_glb_ptr_sStatus->status=0;
        return;
    }

    Tox_Err_File_Send_Chunk err;
    tox_file_send_chunk(m, ft->friendnumber, ft->filenumber, position, send_data, send_length, &err);

    free(send_data);

    if (err != TOX_ERR_FILE_SEND_CHUNK_OK) {
        fprintf(stderr, "tox_file_send_chunk failed in chat callback (error %d)\n", err);
    }

    ft->position += send_length;
    ft->bps += send_length;
}

void on_file_chunk_request_cb(Tox *m, uint32_t friendnumber, uint32_t filenumber, uint64_t position,
                           size_t length, void *userdata)
{
    UNUSED_VAR(userdata);

    onFileChunkRequest(m, friendnumber, filenumber, position, length);
}

/*******************************************************************************
 *
 * SaveFile
 *
 ******************************************************************************/
void try_save_file(Tox *m, struct Friend *f, size_t argv)
{
    long int idx = (long int)argv;//strtol(argv, NULL, 10);

    if ((idx == 0 && (argv!=0)) || idx < 0 || idx >= MAX_FILES) {
        return;
    }


    struct FileTransfer *ft = get_file_transfer_struct_index(f, idx, FILE_TRANSFER_RECV);

    if (!ft) {
        return;
    }

    if (ft->state != FILE_TRANSFER_PENDING) {
        return;
    }

    printf(" Path to savefile %s\n",ft->file_path);
    //to append
    if ((ft->file = fopen(ft->file_path, "a")) == NULL) {
        const char *msg =  "File transfer failed: Invalid download path.";
        close_file_transfer(m, ft, TOX_FILE_CONTROL_CANCEL, msg);
        return;
    }


    Tox_Err_File_Control err;
    tox_file_control(m, f->friend_num, ft->filenumber, TOX_FILE_CONTROL_RESUME, &err);

    if (err != TOX_ERR_FILE_CONTROL_OK) {
        goto on_recv_error;
    }

    printf("Saving file [%ld] as: '%s'\n", idx, ft->file_path);

    //ft->line_id = self->chatwin->hst->line_end->id + 2;
    ft->state = FILE_TRANSFER_STARTED;
    printf ("OK id of file %ld %s\n",idx,ft->file_name);

	stt_glb_ptr_sStatus->curtransfile = ft;
	stt_glb_ptr_sStatus->status=1;
	stt_glb_ptr_sStatus->timeelapsed=0;

    return;

on_recv_error:

    stt_glb_ptr_sStatus->status=0;

    switch (err) {
        case TOX_ERR_FILE_CONTROL_FRIEND_NOT_FOUND:
            return;

        case TOX_ERR_FILE_CONTROL_FRIEND_NOT_CONNECTED:
            return;

        case TOX_ERR_FILE_CONTROL_NOT_FOUND:
            return;

        case TOX_ERR_FILE_CONTROL_SENDQ:
            return;

        default:
            return;
    }
}


void onFileRecv(Tox *m, uint32_t friendnum, uint32_t filenumber, uint64_t file_size, const char *filename, size_t name_length)
{

    struct Friend *f = getfriend(friendnum);
    struct FileTransfer *ft = new_file_transfer(f,friendnum, filenumber, FILE_TRANSFER_RECV, TOX_FILE_KIND_DATA);

    if (!ft) {
        tox_file_control(m, friendnum, filenumber, TOX_FILE_CONTROL_CANCEL, NULL);
        return;
    }

    printf("Friend muon gui file la: %s %d\n", f->name, f->friend_num);

    char sizestr[32];
    bytes_convert_str(sizestr, sizeof(sizestr), file_size);
    printf ("File transfer request for '%s' (%s)\n", filename, sizestr);

    if (!valid_file_name(filename, name_length)) {
        close_file_transfer(m, ft, TOX_FILE_CONTROL_CANCEL, "File transfer failed: Invalid file name.");
        return;
    }

    size_t file_path_buf_size = PATH_MAX + name_length + 1;
    char *file_path = malloc(file_path_buf_size);

    if (file_path == NULL) {
        close_file_transfer(m, ft, TOX_FILE_CONTROL_CANCEL, "File transfer failed: Out of memory.");
        return;
    }

    size_t path_len = name_length;


    snprintf(file_path, file_path_buf_size, "%s/%s",stt_glb_str_tempdir, stt_glb_str_tempfile);


    if (path_len >= file_path_buf_size || path_len >= sizeof(ft->file_path) || name_length >= sizeof(ft->file_name)) {
        close_file_transfer(m, ft, TOX_FILE_CONTROL_CANCEL, "File transfer failed: File path too long.");
        free(file_path);
        return;
    }

    ft->file_size = file_size;
    snprintf(ft->file_path, sizeof(ft->file_path), "%s", file_path);
    snprintf(ft->file_name, sizeof(ft->file_name), "%s", filename);
    tox_file_get_file_id(m, friendnum, filenumber, ft->file_id, NULL);

    free(file_path);

    try_save_file(m,f,ft->index);
}

 void on_file_recv_cb(Tox *tox, uint32_t friendnumber, uint32_t filenumber, uint32_t kind, uint64_t file_size,
                  const uint8_t *filename, size_t filename_length, void *userdata)
{
    UNUSED_VAR(userdata);

    onFileRecv(tox, friendnumber, filenumber, file_size, (char*)filename, filename_length);
}

static void onFileRecvChunk(Tox *m, uint32_t friendnum, uint32_t filenumber, uint64_t position,
                                 const char *data, size_t length)
{
    
    struct Friend *f = getfriend(friendnum);

    struct FileTransfer *ft = get_file_transfer_struct(f, filenumber);

    if (!ft) {
		printf("File NULL\n");
        return;
    }

    if (ft->state != FILE_TRANSFER_STARTED) {
		printf("File state wrong\n");
        return;
    }

    stt_glb_ptr_sStatus->timeelapsed=50;

    char msg[MAX_STR_SIZE];

    if (length == 0) {
        snprintf(msg, sizeof(msg), "File '%s' successfully received.", ft->file_name);
        close_file_transfer(m, ft, -1, msg);
        move_file();
		stt_glb_ptr_sStatus->status=0;        
        return;
    }

    if (ft->file == NULL) {
        snprintf(msg, sizeof(msg), "File transfer for '%s' failed: Invalid file pointer.", ft->file_name);
        close_file_transfer( m, ft, TOX_FILE_CONTROL_CANCEL, msg);
        stt_glb_ptr_sStatus->status=0;
        return;
    }

    if (fwrite(data, length, 1, ft->file) != 1) {
        snprintf(msg, sizeof(msg), "File transfer for '%s' failed: Write fail. %ld", ft->file_name, (long int)length);
        close_file_transfer( m, ft, TOX_FILE_CONTROL_CANCEL, msg);
        stt_glb_ptr_sStatus->status=0;
        return;
    }

    ft->bps += length;
    ft->position += length;
}

void on_file_recv_chunk_cb(Tox *m, uint32_t friendnumber, uint32_t filenumber, uint64_t position,
                        const uint8_t *data, size_t length, void *userdata)
{
    UNUSED_VAR(userdata);

    onFileRecvChunk(m, friendnumber, filenumber, position, (char *) data, length);
}

/*******************************************************************************************************
                                         LOGS
*******************************************************************************************************/

void setup_log(){
	FILE *fp = fopen(stt_glb_str_pathlogfile, "w");
	fprintf(fp, "Begin\n");
    fclose(fp);
}

void write_to_log_file(char *msg){
	FILE *fp = fopen(stt_glb_str_pathlogfile, "a");
	fprintf(fp, " %s\n",msg);
    // close the file
    fclose(fp);
}

void setup_clientid(){
	char* pathlogfile="./clientid.txt";
	FILE *fp = fopen(pathlogfile, "r");

	if(fp!=NULL){
		fread(stt_glb_str_clientid, ID_MAX_SIZE, 1, fp);
		fclose(fp);
	}

	stt_glb_str_clientid[64]='\0';
}

/*******************************************************************************
 *
 * Main
 *
 ******************************************************************************/
void auto_show_contacts() {
    struct Friend *f = friends;
    int n,total=0,i=0,dem=0;
    char *s=NULL,**temp=NULL;
    char delnum[1000][5];

    for (;f != NULL; f = f->next) {
		delnum[dem][0]='-';
    	dem++;
    }

	temp=(char**)malloc(dem*sizeof(char*));
	if(temp==NULL)show_err_and_stop_program("Error in memory allocation\n");
    f = friends;

    for (;f != NULL; f = f->next) {
        temp[i]=(char*)malloc(2048);
        char *hex = bin2hex(f->pubkey, sizeof(f->pubkey));

		n=sprintf (temp[i], "%d %3d  %15.15s  %12.12s %s \n",dem,GEN_INDEX(f->friend_num, TALK_TYPE_FRIEND), f->name, connection_enum2text(f->connection),hex);

		if(strcmp(stt_glb_str_clientid,hex)!=0) {
			snprintf(delnum[i],5,"%d",GEN_INDEX(f->friend_num, TALK_TYPE_FRIEND));
		}
		else stt_glb_hasclient= 1;

		free(hex);
		temp[i][n]='\0';
		total += n;
		i++;
    }

	if((s = (char*)malloc(total+1)) != NULL){
		s[0] = '\0';   // ensures the memory is an empty string
		for (i=0; i<dem; i++) {
			strcat(s,temp[i]);
		}
	} else {
		show_err_and_stop_program("Error in memory allocation\n");
	}

	printf("%s\n",s);

	free(s);

	for (i=0; i<dem; i++) {
			free(temp[i]);
	}

	free(temp);

	for (i=0; i<dem; i++) {
		if(delnum[i][0] != '-') {
			printf("delete %s\n",delnum[i]);
			auto_del(delnum[i]);
		}
	}

	if(stt_glb_hasclient==0){        
		auto_add_noreq(stt_glb_str_clientid);
		stt_glb_hasclient=1;
	}

	if(stt_glb_hasclient==1){
		TalkingTo = 0;
		friendTakingto = getfriend(TalkingTo);
		if(friendTakingto==NULL) show_err_and_stop_program("Friend Error\n");
	}
}

char *poptok(char **strp) {
    static const char *dem = " \t";
    char *save = *strp;
    *strp = strpbrk(*strp, dem);
    if (*strp == NULL) return save;

    *((*strp)++) = '\0';
    *strp += strspn(*strp,dem);
    return save;
}


void setup_server_status(){
	stt_glb_ptr_sStatus = (struct ServerStatus*)malloc(sizeof(struct ServerStatus));
	if(stt_glb_ptr_sStatus==NULL) {printf("Error in memory allocation\n");exit(0);}
	stt_glb_ptr_sStatus->status = 0;
	stt_glb_ptr_sStatus->curtransfile = NULL;
	stt_glb_ptr_sStatus->timeelapsed=0;
	stt_glb_str_curpathtofile[0]='\0';
    snprintf(stt_glb_str_fullresultfile,LINE_MAX_SIZE,"%s/%s",stt_glb_str_tempdir,stt_glb_str_resultfile);
    stt_glb_str_cmdid[0] = '\0';
}

int main() {

    setup_tox();
    setup_clientid();
    setup_server_status();
 
    //can kiem tra xem thu muc sync ton tai chua?
    char *out=(char*)malloc(2048);
    check_dir(0,out);
    ///printf("checkstt_glb_str_maindir: %d",strcmp(rs,"0\n"));//0: equal
    if(strcmp(out,"0\n")!=0) {printf("Error, stt_glb_str_maindir do not exist!");exit(0);}
    memset(out,0,2048);
    check_dir(1,out);
    ///printf("checkstt_glb_str_tempdir: %d",strcmp(rs,"0\n"));//0: equal
    if(strcmp(out,"0\n")!=0) {printf("Error, stt_glb_str_tempdir do not exist!");exit(0);}
    free(out);

    setup_log();
    uint8_t tox_id_bin[TOX_ADDRESS_SIZE];
	tox_self_get_address(tox, tox_id_bin);
	char *hex = "ServerByToxProtocol-myTOXID:";
	write_to_log_file(hex);
	hex=bin2hex(tox_id_bin, sizeof(tox_id_bin));
    write_to_log_file(hex);
    free(hex);
    //change name
    //hex="autotox";
    size_t len = strlen(stt_glb_str_hexname);
    TOX_ERR_SET_INFO err;
    tox_self_set_name(tox, (uint8_t*)stt_glb_str_hexname, strlen(stt_glb_str_hexname), &err);

    if (err != TOX_ERR_SET_INFO_OK) {
        write_to_log_file("! set name failed, errcode");
    }
    else{
		write_to_log_file(stt_glb_str_hexname);
		self.name = realloc(self.name, len + 1);
		strcpy(self.name, stt_glb_str_hexname);
	}

    //log contacts
    hex="#Friends(conctact_index|name|connection|status message):";
    write_to_log_file(hex);
    char buffer[1024];
    struct Friend *f = friends;
    for (;f != NULL; f = f->next) {
		snprintf(buffer, sizeof(buffer), "%3d  %15.15s  %12.12s  %s",GEN_INDEX(f->friend_num, TALK_TYPE_FRIEND), f->name, connection_enum2text(f->connection), f->status_message);
		write_to_log_file(buffer);
		memset(buffer,0,1024);
    }

    /*char *hashmd5 = (char*)malloc(16);
	md5hash("/var/res/share/syncdir/o3.mp4",20000000,hashmd5);
	print_hash((uint8_t*)hashmd5);
	free(hashmd5);*/

    auto_show_contacts();
    
    printf("* Waiting to be online ...\n");

    uint32_t msecs = 0;
    uint32_t v = 0;
    struct timespec pause;

    while (1) {
		if (msecs >= AREPL_INTERVAL) {            
            if(stt_glb_ptr_sStatus->status!=0) stt_glb_ptr_sStatus->timeelapsed += msecs;
            msecs = 0;
            if(stt_glb_ptr_sStatus->status==1){
			    if(stt_glb_ptr_sStatus->timeelapsed > 5000){
                    stt_glb_ptr_sStatus->timeelapsed=0;
                    stt_glb_ptr_sStatus->status=0;
                    close_file_transfer(tox, stt_glb_ptr_sStatus->curtransfile,TOX_FILE_CONTROL_CANCEL,"receiving timeout");
                    printf("Must stop receiving");
                }
            }
            else if (stt_glb_ptr_sStatus->status==2){
                if(stt_glb_ptr_sStatus->timeelapsed > 5000){
                    stt_glb_ptr_sStatus->timeelapsed=0;
                    stt_glb_ptr_sStatus->status=0;
                    close_file_transfer(tox, stt_glb_ptr_sStatus->curtransfile,TOX_FILE_CONTROL_CANCEL,"sending timeout");
                    printf("Must stop sending");
                }
            }
            
            if(stt_glb_hash == 1){
                stt_glb_countforhash ++;
                if(stt_glb_countforhash > 15) {
                    stt_glb_countforhash = 0;
                    int size = (int)file_size(stt_glb_str_fullresultfile);
                    if( size > 0 ) {//file ton tai
                        FILE *fp = fopen(stt_glb_str_fullresultfile,"r");
                        if(fp!=NULL){
                            char buffer[33];
                            int n = fread(buffer,1,32,fp);
                            buffer[n]='\0';
                            fclose(fp);
                            char out[48];
                            snprintf(out,48,"%d 1 7 1 %s",stt_glb_msgid,buffer);
                            printf("out:%s\n",out);
                            stt_glb_hash = 0;
                            stt_glb_countforhash = 0;
                            stt_glb_hashprocessid=-1;
                            tox_friend_send_message(tox, TalkingTo, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *)out, strlen(out), NULL); 
                        }                   
                    }
                    else {
                        char out[6];
                        snprintf(out,6,"%d 1",-99);
                        tox_friend_send_message(tox, TalkingTo, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *)out, strlen(out), NULL);
                    }
                }
            }
		}
	
        tox_iterate(tox, NULL);
        v = tox_iteration_interval(tox);
        msecs += v;
        
        pause.tv_sec = 0;
        pause.tv_nsec = v * 1000000;
        nanosleep(&pause, NULL);

    }

    return 0;
}
