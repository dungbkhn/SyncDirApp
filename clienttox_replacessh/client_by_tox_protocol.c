#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <stdbool.h>

#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>

#include <tox/tox.h>
#include "md5.h"
#include "client_by_tox_protocol_file_transfers.h"

#define UNUSED_VAR(x) ((void) x)

/*******************************************************************************
 *
 * Consts & Macros
 *
 ******************************************************************************/

#define SN_PRINT_MAX_SIZE 1372

#define TRUNCATE_SIZE 1000000 // 1MB

#define LINE_MAX_SIZE 5120  // If input line's length surpassed this value, it will be truncated.

#define FILENAME_MAX_SIZE 256

#define STREAM_MAX_SIZE 256

#define ID_MAX_SIZE 76

#define PORT_RANGE_START 33445     // tox listen port range
#define PORT_RANGE_END   34445

#define AREPL_INTERVAL  30  // Async REPL iterate interval. unit: millisecond.

#define SAVEDATA_AFTER_COMMAND true // whether save data after executing any command

#define ISDIR 4

/// Macros for terminal display


//initphase: action 0-2 - exec cmd, down file, up file
const int maxcmd_actiontypes_phase[4]={2,1,2,1};

struct ClientStatus {
	uint8_t status; //0: ok; 1: sending file; 2:error, wait to cancel sending file; 3: send cmd and wait cmd res; 4: error want to resend cmd; 5: receiving file; 6:error, wait to cancel receiving file;
	struct FileTransfer *curtransfile;
	uint64_t cmdtimeelapsed; //milli seconds
	uint32_t offlinetimeelapsed; //milli seconds
	int cmdnum; //0: no cmd; other: seqnum;
	int actiontype;
	uint64_t actionnum;
	int elementnum;
	int levelnum;//0: root;
	char *cmd;
	char *execpath;
	bool needsleep; // for long sleep.
};

struct uploadStatus {
	int synctruncsize;	
};

//for client
static uint64_t stt_glb_curactionnum;
//static int stt_glb_curelement;
//static char stt_glb_str_predir_name[FILENAME_MAX_SIZE];
static int stt_glb_curcmdnum;
static int stt_glb_repeatcmdnum;
static int stt_glb_repeatactionnum;

static struct ClientStatus *stt_glb_ptr_cStatus=NULL;
static struct uploadStatus *stt_glb_ptr_uStatus=NULL;

//for file transfer
static uint64_t stt_glb_foffset = 0;
static int stt_glb_ts = 0;
static int stt_glb_uptomem = 0;
static uint64_t stt_glb_mtimeforcmd = 0;
static uint64_t stt_glb_mtime_fordown = 0;
static char stt_glb_str_fullpathtofile[LINE_MAX_SIZE];


//for cmd id
static int stt_glb_premsg_id;

//for main
//static const char stt_glb_str_clientmaindir[]="/home/dungnt/TestSyncDir/MainDir";
//static const char stt_glb_str_clienttempdir[]="/home/dungnt/TestSyncDir/.synctempdir";
static char stt_glb_str_clientmaindir[SN_PRINT_MAX_SIZE];//="/home/dungnt/Backup/Store/MySyncDir";
static const char stt_glb_str_clienttempdir[]="../.temp";
static const char stt_glb_str_tempfile[]="tempfile";
static const char stt_glb_str_logclientfile[] = "logclientsynctox.txt";
static const char stt_glb_str_cmdfile[] = "cmd.txt";
static const char stt_glb_str_okfile[] = "okfile";
static const char stt_glb_str_resultfile[] = "resultfile";
static const char stt_glb_str_clientstatusfile[] = "statusfile";
static const char stt_glb_str_nonreadyfile[] = "nonreadyfile";
static char stt_glb_str_fulltempfile[LINE_MAX_SIZE];
static char stt_glb_str_fullcmdfile[LINE_MAX_SIZE];
static char stt_glb_str_fullokfile[LINE_MAX_SIZE];
static char stt_glb_str_fullresultfile[LINE_MAX_SIZE];
static char stt_glb_str_fullstatusfile[LINE_MAX_SIZE];
static char stt_glb_str_fullnonreadyfile[LINE_MAX_SIZE];
static char stt_glb_str_serverid[ID_MAX_SIZE]="3B4F6BE71EBA8FAFE8B056D49D6FF52B355FEF9A70456D70F73FDD80976009178E7AE4320EED";
static int stt_glb_hasserver=0;
static struct Friend *friendTakingto;
static int stt_glb_cmd_waittime = 5000;

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
enum TALK_TYPE { TALK_TYPE_FRIEND, TALK_TYPE_GROUP, TALK_TYPE_COUNT, TALK_TYPE_NULL = UINT32_MAX };

uint32_t TalkingTo = TALK_TYPE_NULL;


/*******************************************************************************
 *
 * Declare Functions
 *
 ******************************************************************************/

 void write_to_log_file(char *msg);
 void write_to_status_file(char *m);
 void select_action_type(int k);
 int find_element_in_directory(char*, char*);
 int start_send_file(Tox *m, uint32_t friendnum, char *fullpathtofile); //tuong dong cmd_sendfile o toxic
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

bool str2uint(char *str, uint32_t *num) {
    char *str_end;
    long l = strtol(str,&str_end,10);
    if (str_end == str || l < 0 ) return false;
    *num = (uint32_t)l;
    return true;
}


const char * connection_enum2text(TOX_CONNECTION conn) {
    switch (conn) {
        case TOX_CONNECTION_NONE:
            return "Offline";
        case TOX_CONNECTION_TCP:
            return "Online(TCP)";
        case TOX_CONNECTION_UDP:
            return "Online(UDP)";
        default:
            return "UNKNOWN";
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


    while (len >= 0 && pathname[len] == '/') {
        path[len--] = '\0';
    }

    char *finalname = strdup(path);

    

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

/*******************************************************************************
 *
 * CMD Functions
 *
 ******************************************************************************/


/*******************************************************************************
 *
 * Friend-Related Functions
 *
 ******************************************************************************/
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



/*******************************************************************************
 *
 * Server-Respond Functions
 *
 ******************************************************************************/


void respond_msg(char *msg){
	printf("statuscode:%s\n",msg);
	write_to_status_file(msg);
}


void respond_up_file(char *msg){
	printf("truncatefilesizefromserver:%s\n",msg);
	char n[16];
    snprintf(n,16,"%s",msg);
    int ts = strtol(n,NULL,10);
    stt_glb_foffset = ts*TRUNCATE_SIZE;
}

void respond_hash(char *msg){
	printf("hashfromremote:%s\n",msg);
	if(msg[0]=='1') {
        write_to_status_file("0");
        FILE *fp = fopen(stt_glb_str_fullresultfile,"w");
        if(fp!=NULL){
            fwrite(msg+2,strlen(msg)-2,1,fp);
            fclose(fp);
        }
    }
    else {
        remove(stt_glb_str_fullresultfile);
        write_to_status_file("1");
    }
}

void respond_down_zero_result_file(){
	printf("downresultfilefromremote:filesize=0\n");
    write_to_status_file("0");
    FILE *fp = fopen(stt_glb_str_fullresultfile,"w");
    if(fp!=NULL){
        fclose(fp);
    }
}

void respond_down_zero_other_file(){
	printf("downotherfilefromremote:filesize=0\n");
    write_to_status_file("0");
    FILE *fp = fopen(stt_glb_str_fullresultfile,"w");
    if(fp!=NULL){
        fclose(fp);
    }
}

/*******************************************************************************
 *
 * Friend-Related Tox Callbacks
 *
 ******************************************************************************/

void friend_message_cb(Tox *tox, uint32_t friend_num, TOX_MESSAGE_TYPE type, const uint8_t *message,
                                   size_t length, void *user_data)
{
    struct Friend *f = getfriend(friend_num);
    if (!f) return;
    if (type != TOX_MESSAGE_TYPE_NORMAL) {
        printf("* receive MESSAGE ACTION type from %s, no supported\n", f->name);
        return;
    }

    char *msg = (char*)message;
	printf("msg received:%s\n", msg);
	char msgid_inchar[4];
	memcpy(msgid_inchar,msg,3);
	msgid_inchar[3]='\0';
	int received_msgid = (int)strtol(msgid_inchar,NULL,10);
	printf("msgid received:%d, premsg_id=%d\n", received_msgid,stt_glb_premsg_id);

	if(received_msgid!=stt_glb_premsg_id) {
        if(received_msgid==-99){
            stt_glb_ptr_cStatus->cmdtimeelapsed = 50;
        }
        else{
			stt_glb_cmd_waittime *= 2;
				
		    printf("Attention!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! receive older respond_msg\n");
		}
		return;
	} else stt_glb_cmd_waittime = 5000;

	if(msg[4]=='1') //cmd ok
	{
		printf("cmd ok\n");
		stt_glb_ptr_cStatus->cmdnum++;
		stt_glb_curcmdnum=stt_glb_ptr_cStatus->cmdnum;
		stt_glb_repeatcmdnum=0;
		size_t msglen = strlen((char*)msg);
		if(msglen >= 6){
			if(msg[6]=='5'){
				respond_msg((char*)msg+8);
			}
            else if(msg[6]=='6'){
				respond_up_file((char*)msg+8);
			}
            else if(msg[6]=='7'){
				respond_hash((char*)msg+8);
			}
            else if(msg[6]=='8'){
				respond_down_zero_result_file();
			}
            else if(msg[6]=='9'){
				respond_down_zero_other_file();
			}
		}
	}
	else{ //run cmd fail, should be logged here
		printf("cmd not ok, should be logged :%s\n",msg);
	}

	printf("cmdnum:%d\n", stt_glb_ptr_cStatus->cmdnum);

	stt_glb_ptr_cStatus->status = 0;
}

void friend_name_cb(Tox *tox, uint32_t friend_num, const uint8_t *name, size_t length, void *user_data) {
    struct Friend *f = getfriend(friend_num);

    if (f) {
        f->name = realloc(f->name, length+1);
        sprintf(f->name, "%.*s", (int)length, (char*)name);
        if (GEN_INDEX(friend_num, TALK_TYPE_FRIEND) == TalkingTo) {
            printf("* Opposite changed name to %.*s\n", (int)length, (char*)name);
            //sprintf(async_repl->prompt, FRIEND_TALK_PROMPT, f->name);
        }
    }
}

void friend_status_message_cb(Tox *tox, uint32_t friend_num, const uint8_t *message, size_t length, void *user_data) {
    struct Friend *f = getfriend(friend_num);
    if (f) {
        f->status_message = realloc(f->status_message, length + 1);
        sprintf(f->status_message, "%.*s",(int)length, (char*)message);
        //printf("co realloc %lu\n",length);
    }
}

void friend_connection_status_cb(Tox *tox, uint32_t friend_num, TOX_CONNECTION connection_status, void *user_data)
{
    struct Friend *f = getfriend(friend_num);
    if (f) {
        f->connection = connection_status;
        printf("* %s is %s\n", f->name, connection_enum2text(connection_status));
    }
}

void friend_request_cb(Tox *tox, const uint8_t *public_key, const uint8_t *message, size_t length, void *user_data) {
    printf("* receive friend request(use `/accept` to see).\n");

    struct Request *req = malloc(sizeof(struct Request));

    req->id = 1 + ((requests != NULL) ? requests->id : 0);
    req->is_friend_request = true;
    memcpy(req->userdata.friend.pubkey, public_key, TOX_PUBLIC_KEY_SIZE);
    req->msg = malloc(length + 1);
    sprintf(req->msg, "%.*s", (int)length, (char*)message);

    req->next = requests;
    requests = req;
}

void self_connection_status_cb(Tox *tox, TOX_CONNECTION connection_status, void *user_data)
{
    self.connection = connection_status;
    printf("* You are %s\n", connection_enum2text(connection_status));
}


/*******************************************************************************
 *
 * SaveFile-RecvFile
 *
 ******************************************************************************/
 void try_save_file(Tox *m, struct Friend *f, size_t argv)
{

    long int idx = (long int)argv;

    if ((idx == 0 && (argv!=0)) || idx < 0 || idx >= MAX_FILES) {
    	printf("No pending file transfers with that ID.\n");
        return;
    }

    struct FileTransfer *ft = get_file_transfer_struct_index(f, idx, FILE_TRANSFER_RECV);

    if (ft->state != FILE_TRANSFER_PENDING) {
        printf("No pending file transfers with that ID.\n");
        const char *msg =  "File transfer failed: No pending file transfers.";
        close_file_transfer(m, ft, TOX_FILE_CONTROL_CANCEL, msg);
        stt_glb_ptr_cStatus->status = 0;
		stt_glb_ptr_cStatus->cmdnum = 1;
		stt_glb_curcmdnum=stt_glb_ptr_cStatus->cmdnum;
		stt_glb_repeatcmdnum=0;
        return;
    }


    if ((ft->file = fopen(ft->file_path, "a")) == NULL) {
        const char *msg =  "File transfer failed: Invalid download path.";
        close_file_transfer(m, ft, TOX_FILE_CONTROL_CANCEL, msg);
        stt_glb_ptr_cStatus->status = 0;
		stt_glb_ptr_cStatus->cmdnum = 1;
		stt_glb_curcmdnum=stt_glb_ptr_cStatus->cmdnum;
		stt_glb_repeatcmdnum=0;
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
    //update client status
    stt_glb_ptr_cStatus->curtransfile = ft;

    return;

on_recv_error:

	stt_glb_ptr_cStatus->status = 0;
	stt_glb_ptr_cStatus->cmdnum = 1;
	stt_glb_curcmdnum=stt_glb_ptr_cStatus->cmdnum;
	stt_glb_repeatcmdnum = 0;

    switch (err) {
        case TOX_ERR_FILE_CONTROL_FRIEND_NOT_FOUND:
            printf("File transfer failed: Friend not found.\n");
            return;

        case TOX_ERR_FILE_CONTROL_FRIEND_NOT_CONNECTED:
            printf("File transfer failed: Friend is not online.\n");
            return;

        case TOX_ERR_FILE_CONTROL_NOT_FOUND:
            printf("File transfer failed: Invalid filenumber.\n");
            return;

        case TOX_ERR_FILE_CONTROL_SENDQ:
            printf("File transfer failed: Connection error.\n");
            return;

        default:
            printf("File transfer failed (error %d)\n", err);
            return;
    }
}


void onFileRecv(Tox *m, uint32_t friendnum, uint32_t filenumber, uint64_t file_size, const char *filename, size_t name_length)
{
    struct Friend *f = getfriend(friendnum);
    struct FileTransfer *ft = new_file_transfer(f,friendnum, filenumber, FILE_TRANSFER_RECV, TOX_FILE_KIND_DATA);

    if (!ft) {
        tox_file_control(m, friendnum, filenumber, TOX_FILE_CONTROL_CANCEL, NULL);
        printf("File transfer request failed: Too many concurrent file transfers.\n");
        stt_glb_ptr_cStatus->status = 0;
		stt_glb_ptr_cStatus->cmdnum = 1;
		stt_glb_curcmdnum=stt_glb_ptr_cStatus->cmdnum;
		stt_glb_repeatcmdnum = 0;

        return;
    }

    printf("Friend muon gui file la: %s %d\n", f->name, f->friend_num);
    printf("FileTransfer la: %ld %d %d\n", ft->index, ft->friendnumber, ft->filenumber);

    char sizestr[32];
    bytes_convert_str(sizestr, sizeof(sizestr), file_size);
    printf ("File transfer request for '%s' (%s)\n", filename, sizestr);

    if (!valid_file_name(filename, name_length)) {
        close_file_transfer(m, ft, TOX_FILE_CONTROL_CANCEL, "File transfer failed: Invalid file name.");
        stt_glb_ptr_cStatus->status = 0;
		stt_glb_ptr_cStatus->cmdnum = 1;
		stt_glb_curcmdnum=stt_glb_ptr_cStatus->cmdnum;
		stt_glb_repeatcmdnum = 0;

        return;
    }

    size_t file_path_buf_size = PATH_MAX + name_length + 1;
    char *file_path = malloc(file_path_buf_size);

    if (file_path == NULL) {
        close_file_transfer(m, ft, TOX_FILE_CONTROL_CANCEL, "File transfer failed: Out of memory.");
        stt_glb_ptr_cStatus->status = 0;
		stt_glb_ptr_cStatus->cmdnum = 1;
		stt_glb_curcmdnum=stt_glb_ptr_cStatus->cmdnum;
		stt_glb_repeatcmdnum = 0;
        return;
    }

    size_t path_len = name_length;

    snprintf(file_path, file_path_buf_size, "%s", filename);


    if (path_len >= file_path_buf_size || path_len >= sizeof(ft->file_path) || name_length >= sizeof(ft->file_name)) {
        close_file_transfer(m, ft, TOX_FILE_CONTROL_CANCEL, "File transfer failed: File path too long.");
        free(file_path);
        stt_glb_ptr_cStatus->status = 0;
		stt_glb_ptr_cStatus->cmdnum = 1;
		stt_glb_curcmdnum=stt_glb_ptr_cStatus->cmdnum;
		stt_glb_repeatcmdnum = 0;

        return;
    }

    char mypath[LINE_MAX_SIZE];

    if(stt_glb_ptr_cStatus->actiontype==0)
	    snprintf(mypath,LINE_MAX_SIZE,"%s/%s",stt_glb_str_clienttempdir,filename);//receive resultfile
	else
        snprintf(mypath,LINE_MAX_SIZE,"%s/%s",stt_glb_str_clienttempdir,stt_glb_str_tempfile);

	printf("savetopath:%s\n",mypath);

    memset(file_path,0,PATH_MAX+1);
    memcpy(file_path,mypath,strlen(mypath));
	file_path[strlen(mypath)] = '\0';

    ft->file_size = file_size;
    snprintf(ft->file_path, sizeof(ft->file_path), "%s", file_path);
    snprintf(ft->file_name, sizeof(ft->file_name), "%s", filename);
    tox_file_get_file_id(m, friendnum, filenumber, ft->file_id, NULL);

    free(file_path);

    try_save_file(m,f, ft->index);
}


void on_file_recv_cb(Tox *tox, uint32_t friendnumber, uint32_t filenumber, uint32_t kind, uint64_t file_size,
                  const uint8_t *filename, size_t filename_length, void *userdata)
{
    UNUSED_VAR(userdata);

    printf("File info: %d %d %d %ld %s %ld\n",friendnumber,filenumber,kind,file_size,filename,filename_length);
    onFileRecv(tox, friendnumber, filenumber, file_size, (char*)filename, filename_length);
}

static void onFileRecvChunk(Tox *m, uint32_t friendnum, uint32_t filenumber, uint64_t position,
                                 const char *data, size_t length)
{
    struct Friend *f = getfriend(friendnum);

    struct FileTransfer *ft = get_file_transfer_struct(f, filenumber);

    if (!ft) {
		printf("File NULL\n");
		tox_file_control(m, friendnum, filenumber, TOX_FILE_CONTROL_CANCEL, NULL);
		stt_glb_ptr_cStatus->cmdnum = 1;
        stt_glb_ptr_cStatus->status = 0;
        stt_glb_curcmdnum=stt_glb_ptr_cStatus->cmdnum;
        stt_glb_repeatcmdnum = 0;

        return;
    }

    if (ft->state != FILE_TRANSFER_STARTED) {
		printf("File state wrong\n");
		close_file_transfer( m, ft, TOX_FILE_CONTROL_CANCEL, "File received failed: File state wrong");
        stt_glb_ptr_cStatus->cmdnum = 1;
        stt_glb_ptr_cStatus->status = 0;
        stt_glb_curcmdnum=stt_glb_ptr_cStatus->cmdnum;
        stt_glb_repeatcmdnum = 0;
        return;
    }

    char msg[MAX_STR_SIZE];

	stt_glb_ptr_cStatus->cmdtimeelapsed = 50;

    if (length == 0) {
		char keepfilename[FILENAME_MAX_SIZE];
		snprintf(keepfilename, FILENAME_MAX_SIZE, "%s", ft->file_name);
        snprintf(msg, sizeof(msg), "File '%s' successfully received.", ft->file_name);  //in dong nay truoc, ben gui in sau
        close_file_transfer(m, ft, -1, msg);

       /* if((stt_glb_ptr_cStatus->actiontype!=1)||(stt_glb_ptr_cStatus->initphase!=0)){//move file
			if(move_file(stt_glb_ptr_cStatus->execpath,keepfilename)==0) show_err_and_stop_program("Error in moving file\n");
		} else { // neu la lsfile.txt
			int kq=assign_elementnum();
			if(kq==0) show_err_and_stop_program("Error reading lstextfile\n");//gan stt_glb_ptr_cStatus->elementnum
		}*/

        if(stt_glb_ptr_cStatus->actiontype!=0) write_to_status_file("0");
        stt_glb_ptr_cStatus->status = 0;
        stt_glb_ptr_cStatus->cmdnum++;
        stt_glb_curcmdnum=stt_glb_ptr_cStatus->cmdnum;
        stt_glb_repeatcmdnum=0;

        return;
    }

    if (ft->file == NULL) {
        snprintf(msg, sizeof(msg), "File transfer for '%s' failed: Invalid file pointer.", ft->file_name);
        close_file_transfer( m, ft, TOX_FILE_CONTROL_CANCEL, msg);
        stt_glb_ptr_cStatus->cmdnum = 1;
        stt_glb_ptr_cStatus->status = 0;
        stt_glb_curcmdnum=stt_glb_ptr_cStatus->cmdnum;
        stt_glb_repeatcmdnum = 0;

        return;
    }

    if (fwrite(data, length, 1, ft->file) != 1) {
        snprintf(msg, sizeof(msg), "File transfer for '%s' failed: Write fail.", ft->file_name);
        close_file_transfer( m, ft, TOX_FILE_CONTROL_CANCEL, msg);
        stt_glb_ptr_cStatus->cmdnum = 1;
        stt_glb_ptr_cStatus->status = 0;
        stt_glb_curcmdnum=stt_glb_ptr_cStatus->cmdnum;
        stt_glb_repeatcmdnum = 0;

        return;
    }

    ft->bps += length;
    ft->position += length;
}

void on_file_recv_chunk_cb(Tox *m, uint32_t friendnumber, uint32_t filenumber, uint64_t position,
                        const uint8_t *data, size_t length, void *userdata)
{
    UNUSED_VAR(userdata);

    //printf("File info: %d %d %ld %ld\n",friendnumber,filenumber,position,length);

    onFileRecvChunk(m, friendnumber, filenumber, position, (char *) data, length);
}


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
 * SendFile-FileReq
 *
 ******************************************************************************/

int start_send_file(Tox *m, uint32_t friendnum, char *fullpathtofile) 
{
    const char *errmsg = NULL;
    struct Friend *f = getfriend(friendnum);

    char path[MAX_STR_SIZE];

    if(stt_glb_uptomem == 1){
        snprintf(path, sizeof(path), "%s", fullpathtofile);
    }
    else {
        snprintf(path, sizeof(path), "%s%s", stt_glb_str_clientmaindir, fullpathtofile);
    }

	printf("filetosend:%s\nstt_glb_foffset:%ld\n",path,stt_glb_foffset);

    /*int path_len = strlen(path);

    if (path_len >= MAX_STR_SIZE) {
        write_to_log_file("File path exceeds character limit.\n");
		printf("File path exceeds character limit.\n");
        return 2;
    }*/

    FILE *file_to_send = fopen(path, "r");

    if (file_to_send == NULL) {
        write_to_log_file("File not found.\n");
		printf("File not found.\n");
        return 2;
    }

    off_t filesize = file_size(path) - (off_t)stt_glb_foffset;

    if (filesize == 0) {
        write_to_log_file("Invalid file.\n");
		printf("Invalid file.\n");
        fclose(file_to_send);
        return 2;
    }

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

    //printf("qua ft %u %s ", filenum,(char *)ft->file_id);
    memcpy(ft->file_name, file_name, namelen + 1);//namelen+1 se copy luon ket thuc xau
    ft->file = file_to_send;
    ft->file_size = filesize;
    tox_file_get_file_id(m, friendnum, filenum, ft->file_id, NULL);

	ft->position = (uint64_t)stt_glb_foffset;
	if (fseek(ft->file, ft->position, SEEK_SET) == -1) {
		goto on_send_error;
	}

    printf("Sending file [%d]: '%s' \n", filenum, file_name);

    stt_glb_ptr_cStatus->curtransfile = ft;
	

    return 1;

on_send_error:

	stt_glb_ptr_cStatus->status = 0;
	stt_glb_ptr_cStatus->cmdnum = 1;
	stt_glb_curcmdnum=stt_glb_ptr_cStatus->cmdnum;
	stt_glb_repeatcmdnum = 0;

    switch (err) {
        case TOX_ERR_FILE_SEND_FRIEND_NOT_FOUND:
            errmsg = "File transfer failed: Invalid friend.";
            break;

        case TOX_ERR_FILE_SEND_FRIEND_NOT_CONNECTED:
            errmsg = "File transfer failed: Friend is offline.";
            break;

        case TOX_ERR_FILE_SEND_NAME_TOO_LONG:
            errmsg = "File transfer failed: Filename is too long.";
            break;

        case TOX_ERR_FILE_SEND_TOO_MANY:
            errmsg = "File transfer failed: Too many concurrent file transfers.";
            break;

        default:
            errmsg = "File transfer failed.";
            break;
    }


    printf("Error %s\n",errmsg);
    tox_file_control(m, friendnum, filenum, TOX_FILE_CONTROL_CANCEL, NULL);
    fclose(file_to_send);

    return 2;
}

static void onFileChunkRequest(Tox *m, uint32_t friendnum, uint32_t filenumber, uint64_t position, size_t length)
{
    struct Friend *f = getfriend(friendnum);

    struct FileTransfer *ft = get_file_transfer_struct(f, filenumber);
    
    if (!ft) {
		write_to_log_file("onFileChunkRequest:null file.\n");
		printf("onFileChunkRequest:null file.\n");
        return;
    }

    if (ft->state != FILE_TRANSFER_STARTED) {
		write_to_log_file("onFileChunkRequest:state file not correct.\n");
		printf("onFileChunkRequest:state file not correct.\n");
        return;
    }

	stt_glb_ptr_cStatus->cmdtimeelapsed = 50;
	uint64_t kp = position;
    char msg[MAX_STR_SIZE];

    if (length == 0) {
        snprintf(msg, sizeof(msg), "File '%s' successfully sent.", ft->file_name);
        close_file_transfer(m, ft, -1, msg);
        stt_glb_ptr_cStatus->status = 0;
        stt_glb_ptr_cStatus->cmdnum++;
        stt_glb_repeatcmdnum=0;
        stt_glb_curcmdnum=stt_glb_ptr_cStatus->cmdnum;
		write_to_status_file("0");

        return;
    }

    if (ft->file == NULL) {
        snprintf(msg, sizeof(msg), "File transfer for '%s' failed: Null file pointer.", ft->file_name);
        close_file_transfer(m, ft, TOX_FILE_CONTROL_CANCEL, msg);
        stt_glb_ptr_cStatus->status = 0;
		stt_glb_ptr_cStatus->cmdnum = 1;
		stt_glb_curcmdnum=stt_glb_ptr_cStatus->cmdnum;
		stt_glb_repeatcmdnum = 0;
        return;
    }

	kp+=(uint64_t)stt_glb_foffset;

    if (ft->position != kp) {
        if (fseek(ft->file, position, SEEK_SET) == -1) {
            snprintf(msg, sizeof(msg), "File transfer for '%s' failed: Seek fail.", ft->file_name);
            close_file_transfer(m, ft, TOX_FILE_CONTROL_CANCEL, msg);
            stt_glb_ptr_cStatus->status = 0;
			stt_glb_ptr_cStatus->cmdnum = 1;
			stt_glb_curcmdnum=stt_glb_ptr_cStatus->cmdnum;
			stt_glb_repeatcmdnum = 0;
            return;
        }
		printf("seek file ok\n");
        ft->position = kp;
    }

    uint8_t *send_data = malloc(length);

    if (send_data == NULL) {
        snprintf(msg, sizeof(msg), "File transfer for '%s' failed: Out of memory.", ft->file_name);
        close_file_transfer(m, ft, TOX_FILE_CONTROL_CANCEL, msg);
        stt_glb_ptr_cStatus->status = 0;
		stt_glb_ptr_cStatus->cmdnum = 1;
		stt_glb_curcmdnum=stt_glb_ptr_cStatus->cmdnum;
		stt_glb_repeatcmdnum = 0;
        return;
    }

    size_t send_length = fread(send_data, 1, length, ft->file);

    if (send_length != length) {
        snprintf(msg, sizeof(msg), "File transfer for '%s' failed: Read fail.", ft->file_name);
        close_file_transfer(m, ft, TOX_FILE_CONTROL_CANCEL, msg);
        free(send_data);
        stt_glb_ptr_cStatus->status = 0;
		stt_glb_ptr_cStatus->cmdnum = 1;
		stt_glb_curcmdnum=stt_glb_ptr_cStatus->cmdnum;
		stt_glb_repeatcmdnum = 0;
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
	
    // self
    tox_callback_self_connection_status(tox, self_connection_status_cb);

    // friend
    tox_callback_friend_request(tox, friend_request_cb);
    tox_callback_friend_message(tox, friend_message_cb);
    tox_callback_friend_name(tox, friend_name_cb);
    tox_callback_friend_status_message(tox, friend_status_message_cb);
    tox_callback_friend_connection_status(tox, friend_connection_status_cb);

    //savefile
    tox_callback_file_recv(tox, on_file_recv_cb);
    tox_callback_file_recv_chunk(tox, on_file_recv_chunk_cb);
    //control
    tox_callback_file_recv_control(tox, on_file_recv_control_cb);
    //sendfile
    tox_callback_file_chunk_request(tox, on_file_chunk_request_cb);
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


/*******************************************************************************************************
                                         LOGS
*******************************************************************************************************/

void setup_log_file(){
	char pathlogfile[LINE_MAX_SIZE];
	snprintf(pathlogfile, LINE_MAX_SIZE, "%s/%s", stt_glb_str_clienttempdir, stt_glb_str_logclientfile);
	FILE *fp = fopen(pathlogfile, "w");
	if(fp==NULL) {
		printf("Error setup log file\n");
		exit(0);
	}
	fprintf(fp, " Begin log\n");
    // close the file
    fclose(fp);
}


void write_to_log_file(char *msg){
	char pathlogfile[LINE_MAX_SIZE];
	snprintf(pathlogfile, LINE_MAX_SIZE, "%s/%s", stt_glb_str_clienttempdir, stt_glb_str_logclientfile);
	FILE *fp = fopen(pathlogfile, "a");
	if(fp==NULL) {
		printf("Error write log file\n");
		exit(0);
	}
	fprintf(fp, " %s\n",msg);
    // close the file
    fclose(fp);
}

void write_to_status_file(char *msg){
	char pathstatusfile[STREAM_MAX_SIZE];
	snprintf(pathstatusfile, STREAM_MAX_SIZE, "%s/%s", stt_glb_str_clienttempdir, stt_glb_str_clientstatusfile);
	FILE *fp = fopen(pathstatusfile, "w");
	if(fp==NULL) {
		printf("Error write log file\n");
		exit(0);
	}
	fprintf(fp, "%s",msg);
    // close the file
    fclose(fp);
}

/*******************************************************************************
 *
 * Server-respond functions
 *
 ******************************************************************************/

void readsyncfolder(){
	char fpath[]="../config/syncfolder";
	if( access( fpath, F_OK ) == 0 ) {//file ton tai
		FILE *fp = fopen(fpath, "r");

		if(fp!=NULL){
			char *line = NULL;
			size_t len = 0;
			//ssize_t read;
			getline(&line, &len, fp);
			line[strlen(line)-1] = '\0';			
			memcpy(stt_glb_str_clientmaindir,line,len);
			printf("syncdir:%s\n",stt_glb_str_clientmaindir);
			free(line);
		}
	} else {
		printf("Error loading config file\n");
		exit(0);
	}
}

int generate_cmd(){
	char fpath[LINE_MAX_SIZE];
	char fokpath[LINE_MAX_SIZE];

    snprintf(fokpath, LINE_MAX_SIZE, "%s/%s", stt_glb_str_clienttempdir, stt_glb_str_okfile);
	snprintf(fpath, LINE_MAX_SIZE, "%s/%s", stt_glb_str_clienttempdir, stt_glb_str_cmdfile);

	if( access( fpath, F_OK ) == 0 ) {//file ton tai
		FILE *fp = fopen(fpath, "r");

		if(fp!=NULL){
			char *line = NULL;
			size_t len = 0;
			//ssize_t read;
			getline(&line, &len, fp);
			line[strlen(line)-1] = '\0';
			printf("line:%s\n",line);
            FILE *fok = fopen(fokpath, "w");
            fwrite("ok",2,1,fok);
            fclose(fok);
			if(strcmp(line,"cmd")==0){
				getline(&line, &len, fp);
				line[strlen(line)-1] = '\0';
				printf("line:%s\n",line);
				snprintf(stt_glb_ptr_cStatus->cmd, LINE_MAX_SIZE, "cmd/%s", line);
				free(line);
				fclose(fp);
				return 3;
			} else if(strcmp(line,"down")==0){
				getline(&line, &len, fp);
				line[strlen(line)-1] = '\0';
				printf("line:%s\n",line);								
				snprintf(stt_glb_str_fullpathtofile, LINE_MAX_SIZE, "%s", line);
                char tempfilepath[LINE_MAX_SIZE];
                snprintf(tempfilepath,LINE_MAX_SIZE,"%s/%s", stt_glb_str_clienttempdir,stt_glb_str_tempfile);                

                struct stat attr;
                stat(stt_glb_str_fullcmdfile, &attr);
                stt_glb_mtimeforcmd = (long)attr.st_mtime;

				free(line);
				fclose(fp);
				select_action_type(1);
				return 100;			
            } else if(strcmp(line,"up")==0){
				getline(&line, &len, fp);
				line[strlen(line)-1] = '\0';
				printf("line:%s\n",line);	

                int i,j=2;
                for(i=0;i<strlen((char*)line);i++)
                    if((line[i]=='/')&&(line[i+1]=='/')) {
                        if(line[i+2]=='.'){
                            j=0;                
                        }
                        else if(line[i+2]==','){
                            j=1;
                        }
                        break;
                    }

                struct stat attr;
                stat(stt_glb_str_fullcmdfile, &attr);
                stt_glb_mtimeforcmd = (long)attr.st_mtime;

                snprintf(stt_glb_str_fullpathtofile, LINE_MAX_SIZE, "%s", line + 5);
                stt_glb_uptomem = j;

				free(line);
				fclose(fp);
				select_action_type(2);
				return 100;
			} else if(strcmp(line,"hash")==0){
				getline(&line, &len, fp);
				line[strlen(line)-1] = '\0';
				printf("line:%s\n",line);		
                struct stat attr;
                stat(stt_glb_str_fullcmdfile, &attr);
                snprintf(stt_glb_ptr_cStatus->cmd, LINE_MAX_SIZE, "hash%ld%s", (long)attr.st_mtime,line);
				free(line);
				fclose(fp);
				select_action_type(3);
				return 100;
			}else if(strcmp(line,"localhash")==0){
                getline(&line, &len, fp);
				line[strlen(line)-1] = '\0';
				printf("line:%s\n",line);	
                long sz = (long)strtol(line,NULL,10);
                getline(&line, &len, fp);
				line[strlen(line)-1] = '\0';
				printf("line:%s\n",line);	

                char *hashmd5 = (char*)malloc(16);
                md5hash(line,sz,hashmd5);          
                char strhash[4];
                //print_hash((uint8_t*)hashmd5);
                
                write_to_status_file("0");
                FILE *fp = fopen(stt_glb_str_fullresultfile,"w");
                for(unsigned int i = 0; i < 16; ++i){
                    memset(strhash,0,4);
                    snprintf(strhash,4,"%02x", (uint8_t)hashmd5[i]);
                    fwrite(strhash,strlen(strhash),1,fp);
                }

                snprintf(strhash,2,"\n");
                fwrite(strhash,strlen(strhash),1,fp);

                free(hashmd5);
                free(line);
				fclose(fp);
                remove(fpath);
                remove(fokpath);
            }
            else {
				free(line);
				fclose(fp);
                remove(fpath);
                remove(fokpath);
			}
		}
	}

	return 0;
}


int truncate_get_size_tempfile(){
	char tempfilepath[LINE_MAX_SIZE];
	char cmd[LINE_MAX_SIZE];
	FILE * stream;
	char buffer[STREAM_MAX_SIZE];
	size_t n=0;

	snprintf(tempfilepath,LINE_MAX_SIZE,"%s/%s", stt_glb_str_clienttempdir,stt_glb_str_tempfile);

	off_t fs = file_size(tempfilepath);

	int ts = fs/TRUNCATE_SIZE;

    ts--;

    if(stt_glb_mtime_fordown == stt_glb_mtimeforcmd){
        if(ts < stt_glb_ts) ts = stt_glb_ts;
    }
    else{
        stt_glb_mtime_fordown = stt_glb_mtimeforcmd;
        if(ts < 0) ts = 0;
        stt_glb_ts = ts;
    }

	snprintf(cmd, LINE_MAX_SIZE, "if [ ! -f %s/%s ] ; then touch %s/%s; echo $?; else truncate --size=%dMB %s/%s; echo $?; fi", stt_glb_str_clienttempdir, stt_glb_str_tempfile, stt_glb_str_clienttempdir, stt_glb_str_tempfile, ts, stt_glb_str_clienttempdir, stt_glb_str_tempfile);

	printf("%d:truncatecmd:%s\n",stt_glb_ts,cmd);

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
 * Main
 *
 ******************************************************************************/


void setup_serverid(){
	char* pathlogfile="./serverid.txt";
	FILE *fp = fopen(pathlogfile, "r");

	if(fp!=NULL){
		fread(stt_glb_str_serverid, ID_MAX_SIZE, 1, fp);
		fclose(fp);
	}

	stt_glb_str_serverid[64]='\0';
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

		if(strcmp(stt_glb_str_serverid,hex)!=0) {
			snprintf(delnum[i],5,"%d",GEN_INDEX(f->friend_num, TALK_TYPE_FRIEND));
		}
		else stt_glb_hasserver = 1;

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

	if(stt_glb_hasserver==0){
		auto_add_noreq(stt_glb_str_serverid);
		stt_glb_hasserver=1;
	}

	if(stt_glb_hasserver==1){
		TalkingTo = 0;
		friendTakingto = getfriend(TalkingTo);
		if(friendTakingto==NULL) show_err_and_stop_program("Friend Error\n");
	}
}

void select_action_type(int k){
	//select actiontype by reading text
	stt_glb_ptr_cStatus->actiontype=k;
	stt_glb_ptr_cStatus->cmdnum=1;
	//stt_glb_curcmdnum=stt_glb_ptr_cStatus->cmdnum;
	//stt_glb_repeatcmdnum=0;
	
}


/* 0; wait; other: client status, no wait */
int check_wait_and_generate_cmd(){

	//which actiontype?
	while(1){
		if(stt_glb_ptr_cStatus->actiontype==0){//send cmd
			if(stt_glb_ptr_cStatus->cmdnum == 1){
				int i = generate_cmd();
                if(i==3){
                    remove(stt_glb_str_fullstatusfile);  
                }
                
                if(i!=0){
                    struct stat attr;
		            stat(stt_glb_str_fullcmdfile, &attr);
		            stt_glb_ptr_cStatus->actionnum=(long)attr.st_mtime;
                    stt_glb_curactionnum=1;
                }
                
				if(i==100) continue;//continue loop for other action type
				
                return i;//return 3 or 0
			}
			else if(stt_glb_ptr_cStatus->cmdnum <= maxcmd_actiontypes_phase[0]){
				remove(stt_glb_str_fullresultfile);                    
				snprintf(stt_glb_ptr_cStatus->cmd, LINE_MAX_SIZE, "%s", "dowf");
				return 5;
			}
			else if ( stt_glb_ptr_cStatus->cmdnum > maxcmd_actiontypes_phase[0]){	
				remove(stt_glb_str_fullcmdfile);                	
				remove(stt_glb_str_fullokfile); 	
				select_action_type(0);
			}
		}
		else if(stt_glb_ptr_cStatus->actiontype==1){//down file
			if(stt_glb_ptr_cStatus->cmdnum <= maxcmd_actiontypes_phase[1]){
				int i = truncate_get_size_tempfile();
                if(i==-1) show_err_and_stop_program("Error in truncating file\n");
                remove(stt_glb_str_fullstatusfile);  
                remove(stt_glb_str_fullresultfile);    
				snprintf(stt_glb_ptr_cStatus->cmd, LINE_MAX_SIZE+8, "down%d%s", i,stt_glb_str_fullpathtofile);				
				return 5;
			}
			else {
				remove(stt_glb_str_fullcmdfile);                	
				remove(stt_glb_str_fullokfile); 
				select_action_type(0);
			}
		}
		else if(stt_glb_ptr_cStatus->actiontype==2){//up file
            if(stt_glb_ptr_cStatus->cmdnum == 1){
                if(stt_glb_uptomem == 2)
				    snprintf(stt_glb_ptr_cStatus->cmd, LINE_MAX_SIZE + 10, "upfl%ld//x//%s", stt_glb_mtimeforcmd, stt_glb_str_fullpathtofile);
                else {
                    int i,j;
                    for(i=0;i<strlen((char*)stt_glb_str_fullpathtofile);i++)
                        if(stt_glb_str_fullpathtofile[i]=='/') {
                            j=i;
                        }
                    snprintf(stt_glb_ptr_cStatus->cmd, LINE_MAX_SIZE + 10, "upfl%ld//.//%s", stt_glb_mtimeforcmd, stt_glb_str_fullpathtofile + j);
                }
				return 3;
			}
			else if(stt_glb_ptr_cStatus->cmdnum <= maxcmd_actiontypes_phase[2]){   
                remove(stt_glb_str_fullstatusfile); 
                remove(stt_glb_str_fullresultfile);             
				int i = start_send_file(tox,INDEX_TO_NUM(TalkingTo),stt_glb_str_fullpathtofile);//1: ok upload;0: none;2:filesize=0
                if(i<2) return i;
                else { //if filesize need to up = 0
                    write_to_status_file("0");
                    remove(stt_glb_str_fullcmdfile);                	
				    remove(stt_glb_str_fullokfile);                      
                    printf("-----------------------filesize=0 or not found or Reading Error--------------------------\n");
                    select_action_type(0);
                }
			}
			else {	
				remove(stt_glb_str_fullcmdfile);                	
				remove(stt_glb_str_fullokfile);              
				select_action_type(0);
			}
		}
        else if(stt_glb_ptr_cStatus->actiontype==3){//get hash
            if(stt_glb_ptr_cStatus->cmdnum <= maxcmd_actiontypes_phase[3]){   
                remove(stt_glb_str_fullstatusfile); 
                remove(stt_glb_str_fullresultfile); 
				return 3;
			}
			else {	
				remove(stt_glb_str_fullcmdfile);                	
				remove(stt_glb_str_fullokfile);              
				select_action_type(0);
			}
		}
		else{
			return 0;
		}
	}
	

	return 0;
}

void end_last_communication(int c){

	if(c==1)//case 1: tox connection lost
	{
		if(stt_glb_ptr_cStatus->status==1){
			printf("N1:Must stop sending file\n");
			stt_glb_ptr_cStatus->cmdtimeelapsed=0;
			stt_glb_ptr_cStatus->status=2;
		}
		else if(stt_glb_ptr_cStatus->status==3) {
			stt_glb_ptr_cStatus->cmdtimeelapsed=0;
			stt_glb_ptr_cStatus->status=4;
			printf("N1:Must send or resend cmd\n");
		}
		else if(stt_glb_ptr_cStatus->status==5) {
			stt_glb_ptr_cStatus->cmdtimeelapsed=0;
			stt_glb_ptr_cStatus->status=6;
			printf("N1:Must stop receving file\n");
		}
	} else {//case 2: otherwise
		if(stt_glb_ptr_cStatus->status==1) {
			if(stt_glb_ptr_cStatus->cmdtimeelapsed > 30000){
				stt_glb_ptr_cStatus->cmdtimeelapsed=0;
				stt_glb_ptr_cStatus->status=2;
				printf("N2:Must stop sending file\n");
			}
		}
		else if(stt_glb_ptr_cStatus->status==3){
			if(stt_glb_ptr_cStatus->cmdtimeelapsed > stt_glb_cmd_waittime){
				stt_glb_ptr_cStatus->cmdtimeelapsed=0;
				stt_glb_ptr_cStatus->status=4;
				printf("N2:Must send or resend cmd\n");
			}
		}
		else if(stt_glb_ptr_cStatus->status==5) {
			if(stt_glb_ptr_cStatus->cmdtimeelapsed > 30000){
				stt_glb_ptr_cStatus->cmdtimeelapsed=0;
				stt_glb_ptr_cStatus->status=6;
				printf("N2:Must stop receving file\n");
			}
		}
	}

	if((stt_glb_ptr_cStatus->status==2)||(stt_glb_ptr_cStatus->status==4)||(stt_glb_ptr_cStatus->status==6)){
		if(stt_glb_ptr_cStatus->status!=4) {
			if(c==1)
				close_file_transfer(tox, stt_glb_ptr_cStatus->curtransfile, TOX_FILE_CONTROL_CANCEL, "lost connection");
			else
				close_file_transfer(tox, stt_glb_ptr_cStatus->curtransfile, TOX_FILE_CONTROL_CANCEL, "timeout");

			stt_glb_ptr_cStatus->cmdnum=1;
			if(stt_glb_ptr_cStatus->cmdnum!=stt_glb_curcmdnum) {
				stt_glb_curcmdnum=stt_glb_ptr_cStatus->cmdnum;
				stt_glb_repeatcmdnum=0;
			}
		}

        stt_glb_premsg_id = (stt_glb_premsg_id+1)%1000;
		if(stt_glb_premsg_id < 100) stt_glb_premsg_id = 100;

        struct stat attr;         
        stat(stt_glb_str_fullcmdfile, &attr);
        if(stt_glb_ptr_cStatus->actionnum==(long)attr.st_mtime){
            printf("Need short sleep ==== \n");
            stt_glb_curactionnum++;
            if(stt_glb_curactionnum == 20){
                stt_glb_ptr_cStatus->offlinetimeelapsed = 0;
                stt_glb_curactionnum = 0;
                write_to_status_file("255");//run cmd/up/down fail
                remove(stt_glb_str_fullokfile); 
                remove(stt_glb_str_fullresultfile);
                remove(stt_glb_str_fullcmdfile); 
            }
        }            
        else printf("Need short sleep\n");

		sleep(0.25);//short sleep
		stt_glb_ptr_cStatus->status=0;
	}

}


void run_iterate(uint32_t msecs){
    static char line[LINE_MAX_SIZE];
    int w = 0; //client status

	if((stt_glb_hasserver==0)||(TalkingTo!=0)||(friendTakingto==NULL)) return;

	if ((friendTakingto->connection==TOX_CONNECTION_NONE)) {
        FILE *fp = fopen(stt_glb_str_fullnonreadyfile,"w");
        if(fp!=NULL) fclose(fp);
        stt_glb_ptr_cStatus->offlinetimeelapsed += msecs;
        if(stt_glb_ptr_cStatus->offlinetimeelapsed > 180000){ //180s
            stt_glb_ptr_cStatus->offlinetimeelapsed = 0;
            stt_glb_curactionnum = 0;
            write_to_status_file("254");//network problem
            remove(stt_glb_str_fullokfile); 
            remove(stt_glb_str_fullresultfile);
            remove(stt_glb_str_fullcmdfile);                	                        
        }
		end_last_communication(1);
		return;
	}

    stt_glb_ptr_cStatus->offlinetimeelapsed = 0;
    remove(stt_glb_str_fullnonreadyfile);
	end_last_communication(2);

    //nhan lenh
	if(stt_glb_ptr_cStatus->status==0)
		w=check_wait_and_generate_cmd();


	if((w!=0)&&(w!=1)){
		//ok gui cmd cho phia server
		stt_glb_premsg_id = (stt_glb_premsg_id+1)%1000;
		if(stt_glb_premsg_id < 100) stt_glb_premsg_id = 100;

		snprintf(line, 10, "%d ",stt_glb_premsg_id);
		memcpy(line+4,stt_glb_ptr_cStatus->cmd,strlen(stt_glb_ptr_cStatus->cmd));
		line[strlen(stt_glb_ptr_cStatus->cmd)+4] = '\0';

		printf("<<<<<<<<<<<%s\n", line);
		//try to close any file transfer before exec cmd
		close_file_transfer(tox,stt_glb_ptr_cStatus->curtransfile,-1,"close all files");
		//send cmd to server
		tox_friend_send_message(tox, INDEX_TO_NUM(TalkingTo), TOX_MESSAGE_TYPE_NORMAL, (uint8_t*)line, strlen(line), NULL);
		//update client status

		stt_glb_ptr_cStatus->status=w;
		stt_glb_ptr_cStatus->cmdtimeelapsed=0;
	}
	else if (w==1){
        stt_glb_ptr_cStatus->status=w;
		stt_glb_ptr_cStatus->cmdtimeelapsed=0;
    }
}


void setup_client_status(){
	stt_glb_ptr_cStatus = (struct ClientStatus*)malloc(sizeof(struct ClientStatus));
	if(stt_glb_ptr_cStatus==NULL) show_err_and_stop_program("Error in memory allocation\n");
	stt_glb_ptr_cStatus->status = 0;
	stt_glb_ptr_cStatus->actiontype=0;
	stt_glb_ptr_cStatus->cmdnum = 1;
	stt_glb_ptr_cStatus->actionnum = 1;
	stt_glb_ptr_cStatus->elementnum = 1;
	stt_glb_ptr_cStatus->levelnum = 0;
	stt_glb_ptr_cStatus->curtransfile = NULL;
	stt_glb_ptr_cStatus->cmdtimeelapsed = 0;
	stt_glb_ptr_cStatus->offlinetimeelapsed = 0;
	stt_glb_ptr_cStatus->needsleep = false;

	stt_glb_ptr_cStatus->cmd=(char*)malloc(LINE_MAX_SIZE);
	if(stt_glb_ptr_cStatus->cmd==NULL) show_err_and_stop_program("Error in memory allocation\n");
	stt_glb_ptr_cStatus->execpath=(char*)malloc(LINE_MAX_SIZE);
	if(stt_glb_ptr_cStatus->execpath==NULL) show_err_and_stop_program("Error in memory allocation\n");
	snprintf(stt_glb_ptr_cStatus->execpath, LINE_MAX_SIZE, "/");

	stt_glb_ptr_uStatus = (struct uploadStatus*)malloc(sizeof(struct uploadStatus));
	if(stt_glb_ptr_uStatus==NULL) show_err_and_stop_program("Error in memory allocation\n");
	stt_glb_ptr_uStatus->synctruncsize = -1;

	stt_glb_repeatcmdnum=0;
	stt_glb_repeatactionnum=0;
	stt_glb_curactionnum=stt_glb_ptr_cStatus->actionnum;
	stt_glb_curcmdnum=stt_glb_ptr_cStatus->cmdnum;
	//stt_glb_curelement = 0;
	//stt_glb_str_predir_name[0]='\0';
	stt_glb_premsg_id=0;
    snprintf(stt_glb_str_fulltempfile,LINE_MAX_SIZE,"%s/%s", stt_glb_str_clienttempdir,stt_glb_str_tempfile);
    snprintf(stt_glb_str_fullresultfile,LINE_MAX_SIZE,"%s/%s", stt_glb_str_clienttempdir,stt_glb_str_resultfile);
    snprintf(stt_glb_str_fullokfile,LINE_MAX_SIZE,"%s/%s", stt_glb_str_clienttempdir,stt_glb_str_okfile);
    snprintf(stt_glb_str_fullstatusfile,LINE_MAX_SIZE,"%s/%s", stt_glb_str_clienttempdir,stt_glb_str_clientstatusfile);
    snprintf(stt_glb_str_fullcmdfile,LINE_MAX_SIZE,"%s/%s", stt_glb_str_clienttempdir,stt_glb_str_cmdfile);
    snprintf(stt_glb_str_fullnonreadyfile,LINE_MAX_SIZE,"%s/%s", stt_glb_str_clienttempdir,stt_glb_str_nonreadyfile);
    remove(stt_glb_str_fullresultfile);  
    remove(stt_glb_str_fullokfile);
    remove(stt_glb_str_fullstatusfile);
    remove(stt_glb_str_fullcmdfile);

    FILE *fp = fopen(stt_glb_str_fullnonreadyfile,"w");
    if(fp!=NULL) fclose(fp);  
}

int main(int argc, char **argv) {

    setup_tox();
	setup_serverid();
	readsyncfolder();
	setup_client_status();
	setup_log_file();

	printf("Client started.\n");

	//auto_add_noreq("34EA7ED0BCA9D4FBF3B003BFBC535CC258A7A5DD7CA64DB0E3202127735DDA480F4FEA3C70FB");
	_print_friend_info(&self, true);

	printf("Server id = %s\n",stt_glb_str_serverid);
    printf("-------Contacts:------\n");

	auto_show_contacts();

	printf("* Waiting to be online ...\n");

	//char *hashmd5 = (char*)malloc(16);
	//md5hash("/home/dungnt/bcd.mp4",20000000,hashmd5);
	//print_hash((uint8_t*)hashmd5);
	//free(hashmd5);

    uint32_t msecs = 0;
    uint32_t v=0;
    struct timespec pause;             
            
    while (1) {
        if (msecs >= AREPL_INTERVAL) {
            if(stt_glb_ptr_cStatus->status!=0) stt_glb_ptr_cStatus->cmdtimeelapsed+=msecs;             
            run_iterate(msecs);    
            msecs = 0;        
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
