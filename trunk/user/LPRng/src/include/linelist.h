/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 ***************************************************************************/



#ifndef _LINELIST_H_
#define _LINELIST_H_ 1

/*
 * arrays of pointers to lines
 */


#define cval(x) (int)(*(unsigned const char *)(x))

struct line_list {
	char **list;	/* array of pointers to lines */
	int count;		/* number of entries */
	int max;		/* maximum number of entries */
};

typedef void (WorkerProc)( struct line_list *args, int input );

/*
 * data structure for job
 */

struct job{
	char sort_key[512]; /* sort key for job */

	/* information about job in key=value format */
	struct line_list info;

	/* data file information
	   This actually an array of line_list structures
		that has the data file information indexed by
		keyword.  There is one of each of these entries
		for each 'block' of datafiles in the original control
		file.
	 */
	struct line_list datafiles;

	/* routing information for a job
		This is really a list of lists for each destination
		The information per destination is accessed by keyword
	 */
	 
	struct line_list destination;
};

/*
 * Types of options that we can initialize or set values of
 */
#define FLAG_K		0
#define INTEGER_K	1
#define	STRING_K	2

/*
 * datastructure for initialization
 */

struct keywords{
	const char *keyword;	/* name of keyword */
	const char *translation;/* translation for display */
	int type;		/* type of entry */
	void *variable;		/* address of variable */
	int  maxval;		/* value of token */
	int  flag;			/* flag for variable */
	const char *default_value;	/* default value */
};

struct jobwords{
    const char **keyword;		/* name of value in job->info */
    int type;			/* type of entry */
    void *variable;		/* address of variable */
	int  maxlen;		/* length of value */
	const char *key;			/* key we use for value */
};

/*
 * Variables
 */
extern struct keywords Pc_var_list[], DYN_var_list[];
/* we need to free these when we initialize */

EXTERN struct line_list
	Config_line_list, PC_filters_line_list,
	PC_names_line_list, PC_order_line_list,
	PC_info_line_list, PC_entry_line_list, PC_alias_line_list,
	/*
	User_PC_names_line_list, User_PC_order_line_list,
	User_PC_info_line_list, User_PC_alias_line_list,
	*/
	All_line_list, Spool_control, Sort_order,
	RawPerm_line_list, Perm_line_list, Perm_filters_line_list,
	Process_list, Tempfiles, Servers_line_list, Printer_list,
	Files, Status_lines, Logger_line_list, RemoteHost_line_list;
EXTERN struct line_list *Allocs[]
#ifdef DEFS
	 ={
	 &Config_line_list, &PC_filters_line_list,
	 &PC_names_line_list, &PC_order_line_list,
	 &PC_info_line_list, &PC_entry_line_list, &PC_alias_line_list,
	/*
	 &User_PC_names_line_list, &User_PC_order_line_list,
	 &User_PC_info_line_list, &User_PC_alias_line_list,
	*/
	 &All_line_list, &Spool_control, &Sort_order,
	 &RawPerm_line_list, &Perm_line_list, &Perm_filters_line_list,
	 &Tempfiles, &Servers_line_list,
	 &Printer_list, &Files, &Status_lines, &Logger_line_list, &RemoteHost_line_list,
	0 }
#endif
	;


/*
 * Constants
 */
EXTERN const char *Option_value_sep DEFINE( = " \t=#@" );
EXTERN const char *Hash_value_sep DEFINE( = "=#" );
EXTERN const char *Whitespace DEFINE( = " \t\n\f" );
EXTERN const char *List_sep DEFINE( = "[] \t\n\f," );
EXTERN const char *Linespace DEFINE( = " \t" );
EXTERN const char *File_sep DEFINE( = " \t,;:" );
EXTERN const char *Strict_file_sep DEFINE( = ";:" );
EXTERN const char *Perm_sep DEFINE( = "=,;" );
EXTERN const char *Arg_sep DEFINE( = ",;" );
EXTERN const char *Name_sep DEFINE( = "|:" );
EXTERN const char *Line_ends DEFINE( = "\n\014\004\024" );
EXTERN const char *Line_ends_and_colon DEFINE( = "\n\014\004\024:" );
EXTERN const char *Printcap_sep DEFINE( = "|:" );
EXTERN const char *Host_sep DEFINE( = "{} \t," );

/* PROTOTYPES */
void lowercase( char *s );
void uppercase( char *s );
char *trunc_str( char *s);
void *malloc_or_die( size_t size, const char *file, int line );
void *realloc_or_die( void *p, size_t size, const char *file, int line );
char *safestrdup (const char *p, const char *file, int line);
char *safestrdup2( const char *s1, const char *s2, const char *file, int line );
char *safeextend2( char *s1, const char *s2, const char *file, int line );
char *safestrdup3( const char *s1, const char *s2, const char *s3,
	const char *file, int line );
char *safeextend3( char *s1, const char *s2, const char *s3,
	const char *file, int line );
char *safeextend4( char *s1, const char *s2, const char *s3, const char *s4,
	const char *file, int line );
char *safestrdup4( const char *s1, const char *s2,
	const char *s3, const char *s4,
	const char *file, int line );
char *safeextend5( char *s1, const char *s2, const char *s3, const char *s4, const char *s5,
	const char *file, int line );
char *safestrdup5( const char *s1, const char *s2,
	const char *s3, const char *s4, const char *s5,
	const char *file, int line );
void Init_line_list( struct line_list *l );
void Free_line_list( struct line_list *l );
void Free_listof_line_list( struct line_list *l );
void Check_max( struct line_list *l, int incr );
char *Add_line_list( struct line_list *l, const char *str,
		const char *sep, int sort, int uniq );
void Merge_line_list( struct line_list *dest, struct line_list *src,
	const char *sep, int sort, int uniq );
void Merge_listof_line_list( struct line_list *dest, struct line_list *src);
void Split( struct line_list *l, const char *str, const char *sep,
	int sort, const char *keysep, int uniq, int trim, int nocomments, const char *escape );
char *Join_line_list( struct line_list *l, const char *sep );
char *Join_line_list_with_sep( struct line_list *l, const char *sep );
void Dump_line_list( const char *title, struct line_list *l );
void Dump_line_list_sub( const char *title, struct line_list *l );
char *Find_str_in_flat( char *str, const char *key, const char *sep );
int Find_first_key( struct line_list *l, const char *key, const char *sep, int *m );
const char *Find_exists_value( struct line_list *l, const char *key, const char *sep );
char *Find_str_value( struct line_list *l, const char *key );
char *Find_casekey_str_value( struct line_list *l, const char *key, const char *sep );
void Set_str_value( struct line_list *l, const char *key, const char *value );
void Set_casekey_str_value( struct line_list *l, const char *key, const char *value );
void Set_flag_value( struct line_list *l, const char *key, long value );
void Set_nz_flag_value( struct line_list *l, const char *key, long value );
void Set_double_value( struct line_list *l, const char *key, double value );
void Set_decimal_value( struct line_list *l, const char *key, long value );
void Remove_line_list( struct line_list *l, int mid );
int Find_flag_value( struct line_list *l, const char *key );
int Find_decimal_value( struct line_list *l, const char *key );
double Find_double_value( struct line_list *l, const char *key );
void Find_tags( struct line_list *dest, struct line_list *l, const char *key );
void Find_default_tags( struct line_list *dest,
	struct keywords *var_list, const char *tag );
void Read_file_list( int required, struct line_list *model, char *str,
	const char *linesep, int sort, const char *keysep, int uniq, int trim,
	int marker, int doinclude, int nocomment, int depth, int maxdepth );
void Read_fd_and_split( struct line_list *list, int fd,
	const char *linesep, int sort, const char *keysep, int uniq,
	int trim, int nocomment );
void Build_printcap_info( 
	struct line_list *names, struct line_list *order,
	struct line_list *list, struct line_list *raw,
	struct host_information *hostname );
char *Select_pc_info( const char *id,
	struct line_list *info,
	struct line_list *aliases,
	struct line_list *names,
	struct line_list *order,
	struct line_list *input,
	int depth, int wildcard );
void Clear_var_list( struct keywords *v, int setv );
void Set_var_list( struct keywords *keys, struct line_list *values );
void Expand_percent( char **var );
void Expand_vars( void );
void Expand_hash_values( struct line_list *hash );
char *Set_DYN( char **v, const char *s );
void Clear_config( void );
void Get_config( int required, char *path );
void Reset_config( void );
void close_on_exec( int fd );
void Getprintcap_pathlist( int required,
	struct line_list *raw, struct line_list *filters,
	char *path );
void Filterprintcap( struct line_list *raw, struct line_list *filters,
	const char *str );
int Check_for_rg_group( char *user );
int Make_temp_fd_in_dir( char **temppath, char *dir );
int Make_temp_fd( char **temppath );
void Clear_tempfile_list(void);
void Unlink_tempfiles(void);
void Remove_tempfiles(void);
void Split_cmd_line( struct line_list *l, char *line );
int Make_passthrough( char *line, const char *flags, struct line_list *passfd,
	struct job *job, struct line_list *env_init );
int Filter_file( int timeout, int input_fd, int output_fd, const char *error_header,
	char *pgm, const char * filter_options, struct job *job,
	struct line_list *env, int verbose );
char *Is_clean_name( char *s );
void Clean_name( char *s );
void Clean_meta( char *t );
void Dump_parms( const char *title, struct keywords *k );
void Dump_default_parms( int fd, const char *title, struct keywords *k );
void Fix_Z_opts( struct job *job );
void Fix_dollars( struct line_list *l, struct job *job, int nosplit, const char *flags );
char *Make_pathname( const char *dir,  const char *file );
int Get_keyval( char *s, struct keywords *controlwords );
const char *Get_keystr( int c, struct keywords *controlwords );
char *Escape( const char *str, int level );
void Escape_colons( struct line_list *list );
void Unescape( char *str );
char *Fix_str( char *str );
int Shutdown_or_close( int fd );
void Fix_bq_format( int format, struct line_list *datafile );

#endif
