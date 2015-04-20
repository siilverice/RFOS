#include "rfos.h"
#include <stdio.h>
//my include
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <glib.h>
#include <time.h>
#include <errno.h>


int seek_meta =  30;
int seek_data =  0;
int num_disk = 0;
char* disk[4] = {NULL};

static gboolean on_handle_get (
    RFOS *object,
    GDBusMethodInvocation *invocation,
    const gchar *key,
    const gchar *outpath) {

    /** Your Code for Get method here **/

    guint err = 0;
    /** End of Get method execution, returning values **/
    rfos_complete_get(object, invocation, err);
    return TRUE;
}

static gboolean on_handle_put (
    RFOS *object,
    GDBusMethodInvocation *invocation,
    const gchar *key,
    const gchar *src) {

    /** Your code for Put method here **/
    guint err;
    GString *str = g_string_new (NULL);
    int i = 0;
    int file_size=0,disk_size=0;
    int start;
    struct stat st;
    FILE *fp;

    if (num_disk<4)
    {
        for (i = 0; i < num_disk; i++)
        {
            //RAID1
            fp = fopen(disk[i], "r+");
            if (fp!=NULL)
            {
                //exist
                fclose(fp);

                stat(disk[i], &st);
                disk_size = (int) st.st_size;
                stat(src, &st);
                file_size = (int) st.st_size;

                if (file_size <= ((80*disk_size)/100)-seek_data && ((20*disk_size)/100)-seek_meta >= 24 )
                {
                    //get size of disk
                    printf("%s size: %d bytes\n", disk[i],disk_size);
                    start = (20*disk_size)/100;

                    //store key and address , byte , access time
                    g_string_assign (str, "");
                    char conv[15];
                    int j=0;
                    g_string_assign (str, key);
                    sprintf(conv, "%d", seek_data);

                    while (conv[j]!='\0')
                    {
                        g_string_append_c (str, conv[j]);
                        j++;
                    }
                    g_string_append (str, ",");

                    j=0;
                    sprintf(conv, "%d", file_size);

                    while (conv[j]!='\0')
                    {
                        g_string_append_c (str, conv[j]);
                        j++;
                    }
                    g_string_append (str, ",");

                    time_t unix_time;
                    time ( &unix_time );
                    sprintf(conv, "%d", (int) unix_time);
                    j=0;
                    while (conv[j]!='\0')
                    {
                        g_string_append_c (str, conv[j]);
                        j++;
                    }
                    g_string_append (str, "|");

                    //read main data
                    //FILE *fp;
                    fp = fopen(src, "r");
                    char ch;
                    GString *data = g_string_new (NULL);
                    while( ( ch = fgetc(fp) ) != EOF )
                        g_string_append_c (data, ch);

                    fclose(fp);

                    //write metadata
                    fp = fopen(disk[i], "r+");
                    fseek( fp, seek_meta, SEEK_SET );
                    fprintf(fp, str->str);
                    
                    //write data
                    fseek( fp, seek_data+start, SEEK_SET );
                    fprintf(fp, data->str);
                    
                    //write " seek_meta,seekdata " to first 8 bytes
                    fseek( fp, 0, SEEK_SET );
                    fprintf(fp, "%d", seek_meta);
                    
                    //write data
                    fseek( fp, 15, SEEK_SET );
                    fprintf(fp, "%d", seek_data);

                    fclose(fp);
                    
                    printf("seek_meta = %d\nseek_data = %d\n", seek_meta,seek_data+start);
                    printf("%s\n", str->str);
                    printf("%s\n", data->str);
                    err = 0;
                }

                else
                {
                    //not enough space
                    err = ENOSPC;
                }
            }
            printf("\n");
        }
        seek_meta = seek_meta + str->len;
        seek_data = seek_data + file_size;

    }
    else if (num_disk==4)
    {
        //RAID10
    }

    /** End of Put method execution, returning values **/
    rfos_complete_put(object, invocation, err);
 
    return TRUE;
}

static void on_name_acquired (GDBusConnection *connection,
    const gchar *name,
    gpointer user_data)
{
    /* Create a new RFOS service object */
    RFOS *skeleton = rfos_skeleton_new ();
    /* Bind method invocation signals with the appropriate function calls */
    g_signal_connect (skeleton, "handle-get", G_CALLBACK (on_handle_get), NULL);
    g_signal_connect (skeleton, "handle-put", G_CALLBACK (on_handle_put), NULL);
    /* Export the RFOS service on the connection as /kmitl/ce/os/RFOS object  */
    g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (skeleton),
        connection,
        "/kmitl/ce/os/RFOS",
        NULL);
}

int main (int arc, char *argv[])
{
    /* Initialize daemon main loop */
    GMainLoop *loop = g_main_loop_new (NULL, FALSE);
    /* Attempt to register the daemon with 'kmitl.ce.os.RFOS' name on the bus */
    g_bus_own_name (G_BUS_TYPE_SESSION,
        "kmitl.ce.os.RFOS",
        G_BUS_NAME_OWNER_FLAGS_NONE,
        NULL,
        on_name_acquired,
        NULL,
        NULL,
        NULL);

    /* Start the main loop */
    num_disk = arc-1;
    int i = 0;

    printf("%d\n", num_disk);
    
    //add name disk
    for (i = 0; i < num_disk; i++)
    {
        disk[i] = argv[i+1];
    }

    //left
    for ( ; i < 4; i++)
    {
        disk[i] = "null";
    }
    printf("disk1 = %s\n", disk[0]);
    printf("disk2 = %s\n", disk[1]);
    printf("disk3 = %s\n", disk[2]);
    printf("disk4 = %s\n", disk[3]);

    g_main_loop_run (loop);
    return 0;
}
