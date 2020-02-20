static int32_t create_main_directory(const char dirname[])
{
    int8_t exit = 0;
    int8_t get_path = 1;
    printf("Where do you want to save the receiving directory? (complete path)\n");
    
    /* get the path from user */
    do {
        if (get_path)
            fgets(directory_path_prefix, PATH_SIZE - 1, stdin);
        DIR* dir = opendir(directory_path_prefix);
        if (dir) {
            /* Directory exists. */
            closedir(dir);
        }
        else if (ENOENT == errno) {
            printf("Could not open the directory. Make sure it exists\n");
            get_path = 1;
            errno = 0;
        }
        else { /* opendir() failed for some other reason. */
            printf("An error occured while opening the directory. Do you want to try again? (Y or N)\n");
            int8_t get_response = 1;
            do {
                char response;
                fgets(&response, 1, stdin);
                if (response == 'Y' || response == 'y') {
                    get_path = 0;
                    get_response = 0;
                }
                else if (response == 'N' || response == 'n') {
                    return -1;
                }
                else {
                    printf("Invallid response... Try again? (Y or N)");
                }
            } while(get_response);
        }
    } while (!exit);
    
    /* create the directory */
    char dirpath[PATH_SIZE];
    sprintf(dirpath, "%s/%s", directory_path_prefix, dirname);
    if (mkdir(dirpath, 0777) == -1) {
        perror("mkdir()");
        return -1;
    }
    
    return 0;
}
