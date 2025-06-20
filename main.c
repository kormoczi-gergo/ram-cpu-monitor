#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h> // pausing execution
#include <gtk/gtk.h> //   gcc main.c -o mtool $(pkg-config --cflags gtk+-3.0 --libs gtk+-3.0)
#include <glib/gtypes.h>

/*functions used*/
int strToDigits(char[]);
gboolean updateRamInfo(gpointer user_data);
gboolean updateCpuInfo(gpointer user_data);

GtkWidget *total, *available, *bar, *used; //global gtk widgets for ram
GtkWidget *cpuUsage, *cpuBar; // global widgets for cpu

int readingPhaseToDo = 0; //counts what cpu stat reading is currently needs to be done
/*cpu data    [0]     [1]   [2]    [3]    [4]    [5]   [6]      [7]    [8]   [9] 
            user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice;    */ 
unsigned long long cpuInfos[20]; //stores 2 set of cpu datas


/*___CREATE_WINDOW__,__EXECUTE_INFO_ACCESS_FUNCTION___*/
int main(int argc, char const *argv[])
{
    GtkWidget *window, *grid;
    gtk_init(&argc, (char ***)&argv);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL); //create window
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL); //destroy window when x clicked

    grid = gtk_grid_new(); //grid layout
    gtk_container_add(GTK_CONTAINER(window), grid);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 50); //some spacing
    gtk_grid_set_row_spacing(GTK_GRID(grid), 3);


    total = gtk_label_new("total memory: "); //total mem label
    gtk_grid_attach(GTK_GRID(grid), total, 0, 0, 1, 1); //attach to grid

    available = gtk_label_new("available memory: "); //free mem label
    gtk_grid_attach(GTK_GRID(grid), available, 0, 1, 1, 1);

    bar = gtk_progress_bar_new(); //bar visualizing memory usage
    gtk_grid_attach(GTK_GRID(grid), bar, 0, 2, 1, 1);

    used = gtk_label_new("used memory: ");
    gtk_grid_attach(GTK_GRID(grid), used,  0, 4, 1, 1);


    cpuUsage = gtk_label_new("Cpu usage: 0.00 %");
    gtk_grid_attach(GTK_GRID(grid), cpuUsage, 1, 0, 1, 1);

    cpuBar = gtk_progress_bar_new();
    gtk_grid_attach(GTK_GRID(grid), cpuBar, 1, 1, 1, 1);


    
    updateRamInfo(NULL); //execute at the start before the 1 second passes
    g_timeout_add(1000, updateRamInfo, NULL); //execute function in a 1 second intervall
    g_timeout_add(2000, updateCpuInfo, NULL);

    gtk_widget_show_all(window); //show all window
    gtk_main(); //runs gtk

    return 0;
}


    /*___ACCESS->_SHOW_RAM_INFOS_ON_WINDOW__*/
gboolean updateRamInfo(gpointer user_data){
    char meminfo[25][100]; //stores needed infos about ram
    FILE *fileptr;

    fileptr = fopen("/proc/meminfo", "r"); //text file of ram infos
    if (fileptr == NULL) {//handlling
        perror("File couldnt be opened");
        return 0;
    }
    for(int i = 0; i < 25; i++){ //store input
        if(fgets(meminfo[i], sizeof(meminfo[i]), fileptr) == NULL){
            break;
        }
    }
    fclose(fileptr);

    int totalRamkB; // these are kB
    int availableRamkB;

    char buffer[100];
    for(int i = 0; i < 25; i++){    //assign input for needed variables
        strncpy(buffer, meminfo[i], sizeof(meminfo[i]));
        if (strncmp(buffer, "MemTotal", sizeof("MemTotal")-1 ) == 0){
            totalRamkB = strToDigits(buffer);
        }
        else if(strncmp(buffer, "MemAvailable", sizeof("MemAvailable")-1 ) == 0){
            availableRamkB = strToDigits(buffer);
        }
    }
    
    double usedRam  = ((float)totalRamkB - (float)availableRamkB);
    double usageRate = usedRam  / (float)totalRamkB; //% of how much is used from total

        /*___OUTPUT___*/
    char totalRamGb[50]; //total ram
    snprintf(totalRamGb, sizeof(totalRamGb), "%.2lf gb", totalRamkB / 1000000.0); //convert int kb -> string gb
    char totalRamText[100] = "Total memory:\t";
    strncat(totalRamText, totalRamGb, sizeof(totalRamGb));
    gtk_label_set_text(GTK_LABEL(total), totalRamText); //output
    
    char availableRamGb[50]; // avaible
    snprintf(availableRamGb, sizeof(availableRamGb),"%.2lf gb", availableRamkB / 1000000.0); //convert int kb -> string gb
    char availableRamText[100] = "Available memory:\t";
    strncat(availableRamText, availableRamGb, sizeof(availableRamGb));
    gtk_label_set_text(GTK_LABEL(available), availableRamText);

    char usageRateText[50]; //usage rate
    snprintf(usageRateText, sizeof(usageRateText), "%.2lf", usageRate * 100);
    strncat(usageRateText, " % used", sizeof(" % used"));
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(bar), usageRate); // ram usage in progress bar
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(bar), usageRateText); //bar on top of text
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(bar), TRUE); //show bar text

    char usedMemoryGb[50];  //used ram gb
    snprintf(usedMemoryGb, sizeof(usedMemoryGb), "%.2lf gb", usedRam / 1000000);
    char usedMemoryText[100] = "Used memory:\t";
    strncat(usedMemoryText, usedMemoryGb, sizeof(usedMemoryGb));
    gtk_label_set_text(GTK_LABEL(used), usedMemoryText);

    return 1; // 1 == G_TRUE  ||  returned ->= keep running the g_timeout
}


    /*_____CPU_INFO_____*/
gboolean updateCpuInfo(gpointer user_data){

    if(readingPhaseToDo == 0){ // empty stored cpu data, store cpu data 1st time

        FILE *fptr;
        fptr = fopen("/proc/stat", "r"); // opening file of cpu datas
        if(fptr == NULL){ //handling
            return 0; //stop this function
        }

        char line[200]; // stores a line
        fgets(line, sizeof(line), fptr);
        sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu", &cpuInfos[0], &cpuInfos[1], &cpuInfos[2], &cpuInfos[3], &cpuInfos[4], &cpuInfos[5], &cpuInfos[6], &cpuInfos[7], &cpuInfos[8], &cpuInfos[9]);

        fclose(fptr);
        readingPhaseToDo = 1;
    }


    else if(readingPhaseToDo == 1){// store the second cycle of cpu data, and calculate difference

        FILE *fptr;
        fptr = fopen("/proc/stat", "r"); // opening file of cpu datas again
        if(fptr == NULL){ //handling
            return 0; //stop this function
        }

        char line[200]; // stores a line
        fgets(line, sizeof(line), fptr);
        sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu", &cpuInfos[10], &cpuInfos[11], &cpuInfos[12], &cpuInfos[13], &cpuInfos[14], &cpuInfos[15], &cpuInfos[16], &cpuInfos[17], &cpuInfos[18], &cpuInfos[19]);

        fclose(fptr);

        unsigned long long totalCpuTime1 = 0; // total cpu time of first reading (sum of all relevant data)
        unsigned long long totalCpuTime2 = 0; // of second reading
        for(int i = 0; i < 8; i ++){
            totalCpuTime1 += cpuInfos[i];
            totalCpuTime2 += cpuInfos[i + 10];
        }

        unsigned long long idleTime1 = cpuInfos[3] + cpuInfos[4]; // time cpu spent on idle (idle + iowait) at the first reading
        unsigned long long idleTime2 = cpuInfos[13] + cpuInfos[14]; // at the second read

        int deltaTotal = totalCpuTime2 - totalCpuTime1; // difference between the first and latest read in total time
        int deltaIdle = idleTime2 - idleTime1; // difference between the first and latest read in idle time

                                        //time sent being busy         /     time being availabel -> percentage
        double cpuUsageFraction = (( (double) deltaTotal - deltaIdle) / (double)deltaTotal );
        double cpuUsagePercentage = cpuUsageFraction * 100;

        /*_____OUTPUT_____*/
        char  cpuUsageText[100];
        snprintf(cpuUsageText, sizeof(cpuUsageText), "Cpu usage: %.2lf", cpuUsagePercentage);
        strncat(cpuUsageText, " %", sizeof(" %"));
        gtk_label_set_text(GTK_LABEL(cpuUsage), cpuUsageText);


        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(cpuBar), cpuUsageFraction);

        readingPhaseToDo = 0;
    }

    return 1;
}


/*__pick integer from a string::*/

int strToDigits(char string[]){ //picks any didgt from string in order and converts it to an int
    char digitToIntBuffer[50]; 
    memset(digitToIntBuffer, 0, sizeof(digitToIntBuffer)); // �12�11�86�24 !
    for(int i = 0; i < strlen(string); i++){
        if(isdigit(string[i])){
            digitToIntBuffer[strlen(digitToIntBuffer)] = string[i];
        }
    }
    int totalRamInt = atoi(digitToIntBuffer);
    return totalRamInt;
}