#include "rfos.h"
#include <stdio.h>
//my include
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <glib.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>




//To do: update access time



int seek_meta =  30;
int seek_data =  0;
int num_disk = 0;
char* disk[4] = {NULL};
int end_set1 = 0;

int free_space_remove = 0;
int full_meta = 0;

void check_old_seek (int mode)
{
    printf("check_old_seek\n");

    int i=0;

    if (mode == 2)
    {
        //start set2 RAID10
        i = 2;
    }

    printf("mode %d\n", i);

    GString *old_meta = g_string_new("NO");
    GString *old_data = g_string_new("NO");
    FILE *fp;
    for (; i < num_disk; i++)
    {
        printf("i = %d\n", i);
        fp = fopen(disk[i],"r");
        if (fp != NULL)
        {
            char tmp;
            while(1)
            {               
                tmp = fgetc(fp);
                if (ftell(fp) > 30)
                {
                    printf("outtttttttttttttttttt\n");
                    break;
                }

                if (tmp == ',')
                {
                    g_string_assign(old_meta,"");
                    g_string_assign(old_data,"");
                    char tmp1;
                    fseek( fp, 0, SEEK_SET );
                    while (1)
                    {
                        tmp1 = fgetc(fp);
                        if (tmp1 == ',')
                        {
                            break;
                        }
                        else
                            g_string_append_c(old_meta,tmp1);

                    }

                    fseek( fp, 15, SEEK_SET );
                    while (1)
                    {
                        tmp1 = fgetc(fp);
                        if (tmp1 == ',')
                        {
                            break;
                        }
                        else
                            g_string_append_c(old_data,tmp1);

                    }
                    break;
                }                 

            }
           
            fclose(fp);

            if (strcmp("NO",old_meta->str) && strcmp("NO",old_data->str))
            {
                printf("old_meta = %s\n", old_meta->str);
                printf("old_data = %s\n", old_data->str);
                seek_meta = atoi(old_meta->str);
                seek_data = atoi(old_data->str);
                printf("seek_meta = %d\n", seek_meta);
                printf("seek_data = %d\n", seek_data);
            }
            else
            {
                printf("seek_meta = %d\n", seek_meta);
                printf("seek_data = %d\n", seek_data);
            }

            break;
        }
        else
            printf("else\n");
    }
}

static gboolean on_handle_get (
    RFOS *object,
    GDBusMethodInvocation *invocation,
    const gchar *key,
    const gchar *outpath) {

    /** Your Code for Get method here **/
    guint err;
    if (strlen(key) != 8)
    {
        err = ENAMETOOLONG;
        rfos_complete_get (object, invocation, err);
        return TRUE;
    }

    //GString *str = g_string_new (NULL);
    int i = 0;
    int /*file_size=0,*/disk_size=0;
    int start;
    struct stat st;
    FILE *fp;
    char local_key[9]={'\0'};
    int match = 0;



        for (i = 0; i < num_disk; i++)
        {
            fp = fopen(disk[i],"r+");
            if (fp!=NULL)
            {
                //disk ok
                stat(disk[i], &st);
                disk_size = (int) st.st_size;
                int j=0;
                int k=0;
                start = (20*disk_size)/100;
                for (j = 0; ; j++)
                {
                    printf("%d\n", j);
                    printf("seek = %d\n", (int)ftell(fp));
                    
                    if (j==0)
                        //first set
                        fseek( fp, 30, SEEK_SET );

                    else
                    {
                        while(1)
                        {
                            if (fgetc(fp)=='|')
                            {
                                break;
                            }
                            if (ftell(fp) > (20*disk_size)/100)
                            {
                                err = ENOENT;
                                break;
                            }
                        }
                        if (ftell(fp) > (20*disk_size)/100)
                        {
                            err = ENOENT;
                            break;
                        }
                    }


                    for (k = 0; k < 8; k++)
                    {
                        //get key in meta data
                        local_key[k] = fgetc(fp);
                    }

                    if (strcmp(key,local_key)==0)
                    {
                        //match
                        err = 0;
                        char tmp;
                        FILE *dest;
                        GString *addr = g_string_new (NULL);
                        GString *size = g_string_new (NULL);
                        int i_addr=0;
                        int i_size=0;

                        printf("%s\n", key);
                        printf("%s\n", local_key);

                        while(1)
                        {
                            tmp = fgetc(fp);
                            if (tmp ==',')
                            {
                                //end of address
                                break;
                            }
                            g_string_append_c(addr,tmp);
                        }
                        i_addr = atoi(addr->str);

                        while(1)
                        {
                            tmp = fgetc(fp);
                            if (tmp ==',')
                            {
                                //end of address
                                break;
                            }
                            g_string_append_c(size,tmp);
                        }
                        i_size = atoi(size->str);

                        time_t unix_time;
                        time ( &unix_time );
                        fprintf(fp, "%d", (int) unix_time );
                        printf("change time = %d\n", (int)unix_time);

                        if (match == 0)
                        {
                            char temp_out;
                            fseek( fp, start+i_addr, SEEK_SET );
                            dest = fopen(outpath,"wb");
                            for (k = 0; k < i_size; k++)
                            {
                                temp_out = fgetc(fp);
                                fwrite(&temp_out , 1 , 1 , dest );
                            }

                            fclose(dest);

                            match = 1;
                        }

                        break;

                    }
                }
            }
        }

        fclose(fp);


    /** End of Get method execution, returning values **/
    rfos_complete_get(object, invocation, err);
    return TRUE;
}

int use_removed_space(int i, int size_file, const gchar *src, const gchar *key, int disk_size)
{
    //find '-'
    printf("!!!!!!!IN HEAR!!!!!!!!!!!\n");
    int start = (20*disk_size)/100;
    printf("start = %d\n", start);
    char tmp;
    FILE *fp;
    int meta=0;
    fp = fopen(disk[i],"r+");
    fseek( fp, 30, SEEK_SET );
    while(1)
    {
        tmp = fgetc(fp);
        if (tmp == '-')
        {
            printf("still have free space!!!\n" );
            //still have free space!!!
            //--------738861,13,1429868791|
            GString *addr = g_string_new (NULL);
            GString *size = g_string_new (NULL);
            int i_addr=0;
            int i_size=0;

            fseek( fp, -1, SEEK_CUR );
            meta = ftell(fp);
            printf("meta = %d\n", meta);
            fseek( fp, 7, SEEK_CUR );

            while(1)
            {
                tmp = fgetc(fp);
                if (tmp == ',')
                {
                    break;
                }
                g_string_append_c(addr,tmp);
            }
            i_addr = atoi(addr->str);

            while(1)
            {
                tmp = fgetc(fp);
                if (tmp == ',')
                {
                    break;
                }
                g_string_append_c(size,tmp);
            }
            i_size = atoi(size->str);
            free_space_remove = i_size;

            if (size_file <= i_size)
            {
                //OK!
                GString *str = g_string_new(NULL);
                char conv[15];
                int j=0;
                g_string_assign (str, key);

                sprintf(conv, "%d", i_addr);
                while (conv[j]!='\0')
                {
                    g_string_append_c (str, conv[j]);
                    j++;
                }
                g_string_append (str, ",");

                j=0;
                sprintf(conv, "%d", size_file);
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

                fseek(fp,meta,SEEK_SET);
                fprintf(fp, str->str );

                printf("free_space_remove - size_file = %d\n", free_space_remove - size_file);
                if (free_space_remove - size_file > 0 )
                {
                    printf("in in in\n");
                    fseek(fp, seek_meta, SEEK_SET);
                    fprintf(fp, "-");
                    fprintf(fp, "%d", free_space_remove - size_file);
                }

                FILE *data_file;
                data_file = fopen(src, "r");
                char ch;
                GString *data = g_string_new (NULL);
                while( ( ch = fgetc(fp) ) != EOF )
                    g_string_append_c (data, ch);

                fclose(data_file);

                fseek( fp, i_addr+start, SEEK_SET );
                fprintf(fp, data->str);


                fclose(fp);
                return 1;
                break;
            }
        }

        if (ftell(fp) > start)
        {
            printf("no free space\n");
            printf("ftell = %d\n", (int)ftell(fp));
            //no free space
            fclose(fp);
            return 0;
        }
    }
}

static gboolean on_handle_put (
    RFOS *object,
    GDBusMethodInvocation *invocation,
    const gchar *key,
    const gchar *src) {

    /** Your code for Put method here **/
    guint err=0;
    if (strlen(key) != 8)
    {
        err = ENAMETOOLONG;
        rfos_complete_put (object, invocation, err);
        return TRUE;
    }

    printf("end_set1 = %d\n", end_set1);

    GString *str = g_string_new (NULL);
    int i = 0;
    int file_size=0,disk_size=0;
    int start;
    struct stat st;
    FILE *fp;
    

    stat(src, &st);
    file_size = (int) st.st_size;

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
                

                if (file_size <= ((80*disk_size)/100)-seek_data )
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

                    if (((20*disk_size)/100)-seek_meta >= str->len)
                    {
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
                        fprintf(fp, "%d", seek_meta + (int)str->len);
                        fprintf(fp, "," );
                               
                        fseek( fp, 15, SEEK_SET );
                        fprintf(fp, "%d", seek_data+file_size);
                        fprintf(fp, "," );

                        fclose(fp);
                        
                        printf("seek_meta = %d\nseek_data = %d\n", seek_meta,seek_data+start);
                        printf("%s\n", str->str);
                        printf("%s\n", data->str);
                        err = 0;
                    }
                }

                else
                {
                    //not enough space
                    if (!use_removed_space(i, file_size, src, key, disk_size))
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
        printf("RAID10\n");
        printf("%d\n", file_size);

        //mirror 1 & 2
        
        if (end_set1 == 0)
        {
            check_old_seek(1);
            stat(disk[0], &st);
            int set1_size = (int) st.st_size;
            stat(disk[2], &st);
            int set2_size = (int) st.st_size;
            if (file_size <= ((80*set1_size)/100)+((80*set2_size)/100))
            {
                for (i = 0; i < 2; i++)
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

                        printf("in set 1\n");

                        if (file_size <= ((80*disk_size)/100)-seek_data )
                        {
                            printf("     seek_data      %d\n", seek_data);
                            printf("     80per      %d\n", ((80*disk_size)/100));
                            printf("start %d\n",start);

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

                            if (((20*disk_size)/100)-seek_meta >= str->len)
                            {
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
                                fprintf(fp, "%d", seek_meta + (int)str->len);
                                fprintf(fp, "," );
                               
                                fseek( fp, 15, SEEK_SET );
                                fprintf(fp, "%d", seek_data+file_size);
                                fprintf(fp, "," );

                                fclose(fp);
                                
                                printf("seek_meta = %d\nseek_data = %d\n", seek_meta,seek_data+start);
                                printf("%s\n", str->str);
                                //printf("%s\n", data->str);
                                err = 0;
                            }
                            else 
                            {
                                //not enough space
                                if (!use_removed_space(i, file_size, src, key, disk_size))
                                {
                                    end_set1 = 1;
                                    seek_meta = 30;
                                    seek_data = 0;
                                    break;
                                }
                            }
                        }
                        else
                        {
                            //not enough space
                            if (!use_removed_space(i, file_size, src, key, disk_size))
                            {
                                end_set1 = 1;
                                seek_meta = 30;
                                seek_data = 0;
                                break;
                            }
                        }
                    }
                }
            }
            else
            {
                err = ENOSPC;
                rfos_complete_put(object, invocation, err);
 
                return TRUE;
            }

            if (end_set1==0)
            {
                seek_meta = seek_meta + str->len;
                seek_data = seek_data + file_size;
            }
        }

        //mirror 2 & 4
        if (end_set1 == 1)
        {
            /*seek_meta = 0;
            seek_data = 0;*/
            err=0;
            printf("Start set 2\n");
            check_old_seek(2);

            stat(disk[0], &st);
            int set1_size = (int) st.st_size;
            stat(disk[2], &st);
            int set2_size = (int) st.st_size;
            if (file_size <= ((80*set1_size)/100)+((80*set2_size)/100))
            {
                for (i = 2; i < 4; i++)
                {
                    fp = fopen(disk[i], "r+");
                    if (fp!=NULL)
                    {
                        //exist
                        fclose(fp);

                        stat(disk[i], &st);
                        disk_size = (int) st.st_size;
                        stat(src, &st);
                        file_size = (int) st.st_size;

                        printf("in set 2\n");

                        if (file_size <= ((80*disk_size)/100)-seek_data )
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

                            if (((20*disk_size)/100)-seek_meta >= str->len)
                            {
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
                                fprintf(fp, "%d", seek_meta + (int)str->len);
                                fprintf(fp, "," );
                               
                                fseek( fp, 15, SEEK_SET );
                                fprintf(fp, "%d", seek_data+file_size);
                                fprintf(fp, "," );

                                fclose(fp);
                                
                                printf("seek_meta = %d\nseek_data = %d\n", seek_meta,seek_data+start);
                                printf("%s\n", str->str);
                                //printf("%s\n", data->str);
                                err = 0;
                            }
                            else 
                            {
                                //not enough space
                                printf("else 1\n");
                                if (!use_removed_space(1, file_size, src, key, disk_size))
                                {
                                    err = ENOSPC;
                                    end_set1=0;
                                    rfos_complete_put(object, invocation, err);
     
                                    return TRUE;
                                }
                            }
                        }
                        else
                        {
                            //not enough space
                            printf("else 2\n");
                            if (!use_removed_space(1, file_size, src, key, disk_size))
                            {
                                err = ENOSPC;
                                end_set1=0;
                                rfos_complete_put(object, invocation, err);
     
                                return TRUE;
                            }
                        }
                    }
                }
            }
            else
            {
                printf("else else\n");
                err = ENOSPC;
                rfos_complete_put(object, invocation, err);
 
                return TRUE;
            }
            seek_meta = seek_meta + str->len;
            seek_data = seek_data + file_size;

            end_set1=0;
        }
        
    }

    printf("%d\n", err);

    /** End of Put method execution, returning values **/
    rfos_complete_put(object, invocation, err);
 
    return TRUE;
}

gboolean iter_all(gpointer key, gpointer value, gpointer data) {
    g_string_append (data, key);
    g_string_append_c (data, ',');

    return FALSE;
}

static gboolean on_handle_search (
    RFOS *object, 
    GDBusMethodInvocation *invocation,
    const gchar *key, 
    const gchar *outpath ) 
{
    guint err;
    if (strlen(key) > 8)
    {
        err = ENAMETOOLONG;
        rfos_complete_search (object, invocation, err);
        return TRUE;
    }

    int i=0,j=0;
    FILE *fp;
    int disk_size=0;
    struct stat st;
    int found = 0,end = 0;
    char local_key[9]={'\0'};

    for (i = 0; i < num_disk; i++)
    {
        fp = fopen(disk[i],"r");
        if (fp!=NULL)
        {
            printf("disk %d\n", i+1);
            //disk ok
            stat(disk[i], &st);
            disk_size = (int) st.st_size;
            int k=0;
            GTree* t = g_tree_new((GCompareFunc)g_ascii_strcasecmp);

            for (j = 0; ; j++)
            {
                printf("%d\n", j);
                printf("seek = %d\n", (int)ftell(fp));
                    
                if (j==0)
                    //first set
                    fseek( fp, 30, SEEK_SET );

                else
                {
                    while(1)
                    {
                        if (fgetc(fp)=='|')
                        {
                            break;
                        }
                        if (ftell(fp) > (20*disk_size)/100 && found!=1)
                        {
                            err = ENOENT;
                            g_tree_destroy(t);
                            rfos_complete_search (object, invocation, err);
                            return TRUE;
                            break;

                        }
                        if (ftell(fp) > (20*disk_size)/100 && found!=0)
                        {
                            end = 1;
                            break;
                        }
                    }
                }

                if (found==1 && end == 1)
                {
                    err = 0;
                    FILE *dest;
                    GString *str = g_string_new (NULL);
                    g_tree_foreach(t, (GTraverseFunc)iter_all, str);

                    dest = fopen(outpath,"w+");
                    printf("%s\n", str->str);
                    fwrite(str->str , 1 , str->len-1 , dest );
                    fclose(dest);

                    g_tree_destroy(t);
                    break;
                }


                for (k = 0; k < 8; k++)
                {
                    //get key in meta data
                    local_key[k] = fgetc(fp);
                }

                if (g_str_has_prefix(local_key, key))
                {
                    //match
                    char *temp_local_key = (char *) malloc(9);
                    int z=0;
                    for (z = 0; z < 8; z++)
                    {
                        temp_local_key[z] = local_key[z];
                    }
                    temp_local_key[8] = '\0';
                    g_tree_insert(t, temp_local_key , temp_local_key);
                    printf("%s\n", local_key);
                    //g_tree_foreach(t, (GTraverseFunc)iter_all, NULL);

                    found = 1;

                }

            }

            fclose(fp);
            break;
        }
    }

    rfos_complete_search (object, invocation, err);
    return TRUE;
}

static gboolean on_handle_stat (
    RFOS *object, 
    GDBusMethodInvocation *invocation,
    const gchar *key ) 
{
    guint err;
    guint size = 0;
    gint64 atime = 0;
    if (strlen(key) != 8)
    {
        err = ENAMETOOLONG;
        rfos_complete_stat (object, invocation, size, atime, err);
        return TRUE;
    }

    int i = 0;
    int disk_size=0;
    struct stat st;
    FILE *fp;
    char local_key[9]={'\0'};



        for (i = 0; i < num_disk; i++)
        {
            fp = fopen(disk[i],"r");
            if (fp!=NULL)
            {
                //disk ok
                stat(disk[i], &st);
                disk_size = (int) st.st_size;
                int j=0;
                int k=0;
                for (j = 0; ; j++)
                {
                    
                    if (j==0)
                        //first set
                        fseek( fp, 30, SEEK_SET );

                    else
                    {
                        while(1)
                        {
                            if (fgetc(fp)=='|')
                            {
                                break;
                            }
                            if (ftell(fp) > (20*disk_size)/100)
                            {
                                err = ENOENT;
                                break;
                            }
                        }
                        if (ftell(fp) > (20*disk_size)/100)
                        {
                            err = ENOENT;
                            break;
                        }
                    }


                    for (k = 0; k < 8; k++)
                    {
                        //get key in meta data
                        local_key[k] = fgetc(fp);
                    }

                    if (strcmp(key,local_key)==0)
                    {
                        //match
                        err = 0;
                        char tmp;
                        GString *size = g_string_new (NULL);
                        GString *atime = g_string_new (NULL);
                        guint i_size=0;
                        guint i_atime=0;

                        while(1)
                        {
                            tmp = fgetc(fp);
                            if (tmp ==',')
                            {
                                //end of address
                                break;
                            }
                        }

                        while(1)
                        {
                            tmp = fgetc(fp);
                            if (tmp ==',')
                            {
                                //end of address
                                break;
                            }
                            g_string_append_c(size,tmp);
                        }
                        i_size = atoi(size->str);

                        while(1)
                        {
                            tmp = fgetc(fp);
                            if (tmp =='|')
                            {
                                //end of address
                                break;
                            }
                            g_string_append_c(atime,tmp);
                        }
                        i_atime = atoi(atime->str);


                        rfos_complete_stat (object, invocation, i_size, i_atime, err);
                        return TRUE;

                        break;

                    }
                }
            }
        }

        fclose(fp);

    rfos_complete_stat (object, invocation, size, atime, err);
    return TRUE;
}

static gboolean on_handle_remove (
    RFOS *object, 
    GDBusMethodInvocation *invocation,
    const gchar *key )
{
    guint err = 0;

    //-size----------------
    //if size of file < free size => append end of meta
    if (strlen(key) != 8)
    {
        err = ENAMETOOLONG;
        rfos_complete_remove (object, invocation, err);
        return TRUE;
    }

    int i = 0;
    int disk_size=0;
    struct stat st;
    FILE *fp;
    char local_key[9]={'\0'};



        for (i = 0; i < num_disk; i++)
        {
            fp = fopen(disk[i],"r+");
            if (fp!=NULL)
            {
                //disk ok
                stat(disk[i], &st);
                disk_size = (int) st.st_size;
                int j=0;
                int k=0;
                for (j = 0; ; j++)
                {
                    
                    if (j==0)
                        //first set
                        fseek( fp, 30, SEEK_SET );

                    else
                    {
                        while(1)
                        {
                            if (fgetc(fp)=='|')
                            {
                                break;
                            }
                            if (ftell(fp) > (20*disk_size)/100)
                            {
                                err = ENOENT;
                                break;
                            }
                        }
                        if (ftell(fp) > (20*disk_size)/100)
                        {
                            err = ENOENT;
                            break;
                        }
                    }


                    for (k = 0; k < 8; k++)
                    {
                        //get key in meta data
                        local_key[k] = fgetc(fp);
                    }

                    //0000000A0,738861,1429868783|
                    if (strcmp(key,local_key)==0)
                    {
                        err = 0;
                        char tmp;

                        fseek( fp, -8, SEEK_CUR );
                        for (k = 0; k < 8; k++)
                        {
                            fwrite("-",1,1,fp);
                        }

                        while(1)
                        {
                            tmp = fgetc(fp);
                            if (tmp ==',')
                            {
                                //end of addr
                                break;
                            }
                        }

                        while(1)
                        {
                            tmp = fgetc(fp);
                            if (tmp ==',')
                            {
                                //end of size
                                break;
                            }
                        }

                        break;

                    }
                }
            }
        }

        fclose(fp);


    rfos_complete_remove (object, invocation, err);
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
    g_signal_connect (skeleton, "handle-search", G_CALLBACK (on_handle_search), NULL);
    g_signal_connect (skeleton, "handle-stat", G_CALLBACK (on_handle_stat), NULL);
    g_signal_connect (skeleton, "handle-remove", G_CALLBACK (on_handle_remove), NULL);
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

    check_old_seek(1);

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
