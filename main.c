#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <gtk/gtk.h> //   gcc analytics.c -o mtool $(pkg-config --cflags gtk+-3.0 --libs gtk+-3.0)
#include <glib/gtypes.h>
#include <stdbool.h>
#include <sys/stat.h> //for opening a folder
#include <sys/types.h>

#define kbToGbExchange / 1000000.0 //convert amount from Kb to Gb

/*functions used*/
int strToDigits(char[]);
void storeStatistics(const char[], const double);
gboolean updateRamInfo(gpointer user_data);
gboolean updateCpuInfo(gpointer user_data);
gboolean changeAnalyticsBool(gpointer user_data);
gboolean analyzeStoredData(gpointer user_data);
gboolean clearStoredDatas(gpointer user_data);


GtkWidget *total, *available, *bar, *used; //global gtk widget for ram
GtkWidget *cpuUsage, *cpuBar; // global widget for cpu
GtkWidget *writeAnalyticsButton, *analyzeButton, *clearStoredDataButton; //button to do analytycs
GtkWidget *avgCpu, *avgRam; //analytic output label


int readingPhaseToDo = 0; //counts what cpu stat reading is currently needs to be done

/*cpu data    [0]     [1]   [2]    [3]    [4]    [5]   [6]      [7]    [8]   [9] 
            user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice;    */ 
unsigned long long cpuInfos[20]; //stores 2 set of cpu datas

bool storeBool = false;


void createReprository(){ //create a folder used for storing statistics
    char* homepath; 
    homepath = getenv("HOME");
    char folderpath[100];
    snprintf(folderpath, sizeof(folderpath), "%s/z_mtool", homepath);

    #ifdef __linux__
        mkdir(folderpath, 0777);
    #else
        _mkdir(folderpath);
    #endif

}




/*___CREATE_WINDOW__,__EXECUTE_INFO_ACCESS_FUNCTION___*/
int main(int argc, char const *argv[])
{
    createReprository();

    char *userPath;
    userPath = getenv("HOME");

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
    gtk_grid_attach(GTK_GRID(grid), cpuUsage, 0, 5, 1, 1);

    cpuBar = gtk_progress_bar_new();
    gtk_grid_attach(GTK_GRID(grid), cpuBar, 0, 6, 1, 1);


    //analytic buttons
    writeAnalyticsButton = gtk_button_new_with_label("write analytics");
    gtk_grid_attach(GTK_GRID(grid), writeAnalyticsButton, 1, 0, 1, 1);
    g_signal_connect(writeAnalyticsButton, "clicked", G_CALLBACK(changeAnalyticsBool), NULL);

    analyzeButton = gtk_button_new_with_label("analyze stored data");
    gtk_grid_attach(GTK_GRID(grid), analyzeButton, 1, 1, 1, 1);
    g_signal_connect(analyzeButton, "clicked", G_CALLBACK(analyzeStoredData), NULL);

    clearStoredDataButton = gtk_button_new_with_label("erease stored data from storage");
    gtk_grid_attach(GTK_GRID(grid), clearStoredDataButton, 1, 2, 1, 1);
    g_signal_connect(clearStoredDataButton, "clicked", G_CALLBACK(clearStoredDatas), NULL);

    //analytic output
    avgCpu = gtk_label_new("\0");
    gtk_grid_attach(GTK_GRID(grid), avgCpu, 1, 3, 1, 1);

    avgRam = gtk_label_new("\0");
    gtk_grid_attach(GTK_GRID(grid), avgRam, 1, 4, 1, 1);


    
    updateRamInfo(NULL); //execute at the start before the 1 second passes
    g_timeout_add(1000, updateRamInfo, NULL); //execute function in a 1 second intervall
    updateCpuInfo(NULL);
    g_timeout_add(1500, updateCpuInfo, NULL);

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

    char buffer[200];
    for(int i = 0; i < 25; i++){    //assign input for needed variables
        memset(buffer, 0, sizeof(buffer));
        strncpy(buffer, meminfo[i], sizeof(buffer) - 1);
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
    char totalRamGb[100]; //total ram
    snprintf(totalRamGb, sizeof(totalRamGb), "Total memory:\t%.2lf gb", totalRamkB kbToGbExchange); //convert int kb -> string gb
    gtk_label_set_text(GTK_LABEL(total), totalRamGb); //output
    
    char availableRamGb[100]; // avaible
    snprintf(availableRamGb, sizeof(availableRamGb),"Available memory:\t%.2lf gb", availableRamkB kbToGbExchange); //convert int kb -> string gb
    gtk_label_set_text(GTK_LABEL(available), availableRamGb);

    char usageRateText[50]; //usage rate
    snprintf(usageRateText, sizeof(usageRateText), "%.2lf%% used", usageRate * 100);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(bar), usageRate); // ram usage in progress bar
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(bar), usageRateText); //bar on top of text
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(bar), TRUE); //show bar text

    char usedMemoryGb[50];  //used ram gb
    snprintf(usedMemoryGb, sizeof(usedMemoryGb), "Used memory:\t%.2lf gb", usedRam kbToGbExchange);
    gtk_label_set_text(GTK_LABEL(used), usedMemoryGb);


    //store statistics into a text file
    if(storeBool == true){
        storeStatistics("/ramdata.txt", usageRate * 100);
    }

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

                                        //time sent being busy         /     time being available -> percentage
        double cpuUsageFraction = (( (double) deltaTotal - deltaIdle) / (double)deltaTotal );
        double cpuUsagePercentage = cpuUsageFraction * 100;

        /*_____OUTPUT_____*/
        char  cpuUsageText[100];
        snprintf(cpuUsageText, sizeof(cpuUsageText), "Cpu usage: %.2lf %%", cpuUsagePercentage);
        gtk_label_set_text(GTK_LABEL(cpuUsage), cpuUsageText);
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(cpuBar), cpuUsageFraction);


        //storing statistics into a text file
        if(storeBool == true){
            storeStatistics("/cpudata.txt", cpuUsagePercentage);
        }

        readingPhaseToDo = 0;
    }
    
    return 1;
}





    /*change the status of storing-or not storing datas*/

gboolean changeAnalyticsBool(gpointer user_data){
    if(storeBool == false){
        storeBool = true;
        gtk_button_set_label(GTK_BUTTON(writeAnalyticsButton), "stop writing analytics");
    }
    else{
        storeBool = false;
        gtk_button_set_label(GTK_BUTTON(writeAnalyticsButton), "write analytics");
    }
    return 1;
}



    /*Analytics of average usage - output to labels*/

gboolean analyzeStoredData(gpointer user_data){
    FILE *fileptr;
    char *userPath;
    userPath = getenv("HOME");
    char buffer[20];


    //CPU
    char outputCpuPath[100] = "\0"; //opening file
    snprintf(outputCpuPath, sizeof(outputCpuPath), "%s/z_mtool/cpudata.txt", userPath);

    fileptr = fopen(outputCpuPath, "r");
    int i = 0; //number of data
    long double sum = 0; //summ of datas
    while(fgets(buffer, sizeof(buffer), fileptr) != NULL){
        char *ptr;
        sum += strtod(buffer, &ptr);
        i++;
    }
    float avg = sum / i;

    //output
    char avgStr[315] = "\0"; //stores average usage for a specified part in a formatted string
    snprintf(avgStr, sizeof(avgStr), "average cpu usage: %.3lf %%", avg); //converting double to string
    gtk_label_set_text(GTK_LABEL(avgCpu), avgStr);
    fclose(fileptr);


    //RAM
    char ramOutputPath[100] = "\0";    //open file
    snprintf(ramOutputPath, sizeof(ramOutputPath), "%s/z_mtool/ramdata.txt", userPath);

    fileptr = fopen(ramOutputPath, "r");
    i = 0; //count number of data
    sum = 0; //summarize data
    while(fgets(buffer, sizeof(buffer), fileptr) != NULL){
        char *ptr;
        sum += strtod(buffer, &ptr);
        i++;
    }
    avg = sum / i;
    
    //output
    memset(avgStr, 0, sizeof(avgStr)); //empty string
    snprintf(avgStr, sizeof(avgStr), "average ram usage: %.3lf %%", avg);
    gtk_label_set_text(GTK_LABEL(avgRam), avgStr);
    fclose(fileptr);

    return 1;
}





/*Clear stored datas*/
gboolean clearStoredDatas(gpointer user_data){
    FILE *fileptr;
    char *userPath;
    userPath = getenv("HOME");

    //clear ram data
    char outputFilePath[200] = "\0";
    snprintf(outputFilePath, sizeof(outputFilePath), "%s/z_mtool/ramdata.txt", userPath);

    fileptr = fopen(outputFilePath, "w");
    if(fileptr == NULL){ //handling
        return 0;
    }
    fclose(fileptr);

    //clear cpu data
    memset(outputFilePath, 0, sizeof(outputFilePath));
    snprintf(outputFilePath, sizeof(outputFilePath), "%s/z_mtool/cpudata.txt", userPath);

    fileptr = fopen(outputFilePath, "w");
    if(fileptr == NULL){
        printf("file is not reachable");
        return 0;
    }
    fclose(fileptr);

    return 1;
}





/*store statistics into a text file*/

void storeStatistics(const char infoType[40] , const double usage){
    FILE *fileptr;
    char *userPath;
    userPath = getenv("HOME");

    //creating outputpath
    char outputPath[100] = "\0";
    snprintf(outputPath, sizeof(outputPath), "%s/z_mtool%s", userPath, infoType);

    //opening output file
    fileptr = fopen(outputPath, "a");
    if(fileptr == NULL){ //handling
        perror("output couldnt be opened");
    }

    //write output
    fprintf(fileptr, "%.2lf\n", usage);
    fclose(fileptr);
}





/*__pick integer from a string::*/

int strToDigits(char string[]){ //picks digits from string in order and converts it to an int
    char digitToIntBuffer[50]; 
    memset(digitToIntBuffer, 0, sizeof(digitToIntBuffer)); // �12�11�86�24 !  (reset the buffer-> no junk)
    for(int i = 0; i < strlen(string); i++){
        if(isdigit(string[i])){
            digitToIntBuffer[strlen(digitToIntBuffer)] = string[i];
        }
    }
    int totalRamInt = atoi(digitToIntBuffer);
    return totalRamInt;
}
