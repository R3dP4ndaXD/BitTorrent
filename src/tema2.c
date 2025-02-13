#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "tema2.h"

void shuffle(int arr[], int size) {
    for (int i = size - 1; i > 0; i--) {
        // Generate a random index from 0 to i
        int j = rand() % (i + 1);

        // Swap arr[i] with arr[j]
        int temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
    }
}

void print_file_info(File_info *files, int count, int rank, const char *label) {
    printf("[Rank %d] %s (%d files):\n", rank, label, count);
    for (int i = 0; i < count; i++) {
        printf("  File %d: %s\n", i, files[i].filename);
        printf("    Number of Hashes: %d\n", files[i].nr_hashes);
        for (int j = 0; j < files[i].nr_hashes; j++) {
            printf("    Hash %d: %s\n", j, files[i].hashes[j]);
        }
    }
}

void print_file_c(File_c *files, int count, int rank, const char *label) {
    printf("[Rank %d] %s (%d files):\n", rank, label, count);
    for (int i = 0; i < count; i++) {
        printf("  File %d: %s\n", i, files[i].filename);
        printf("    Number of Hashes: %d\n", files[i].nr_hashes);
        for (int j = 0; j < files[i].nr_hashes; j++) {
            printf("    Hash %d: %s\n", j, files[i].hashes[j]);
        }
    }
}

void write_output_file(int rank, File_c* file) {
    char output_filename[128];
    snprintf(output_filename, sizeof(output_filename), "client%d_%s", rank, file->filename);
    FILE *output_file = fopen(output_filename, "w");
    if (file == NULL) {
        perror("Error opening file");
        return;
    }
    //fprintf(output_file, "%d\n", file->nr_hashes);
    for (int i = 0; i < file->nr_hashes; i++) {
        fprintf(output_file, "%s\n", file->hashes[i]);
    }

    fclose(output_file);
}

void *download_thread_func(void *arg)
{
    Download_args *args = (Download_args *)arg;
    char *swarm = calloc(args->nr_downloads, sizeof(char));
    char *msg = calloc(MAX_FILENAME + 1, sizeof(char));
    MPI_Status status;
    
    for (int i = 0; i < args->nr_downloads; i++) {
        File_c *file = &args->downloads[i];
        File_info file_info;
        MPI_Send(file->filename, MAX_FILENAME, MPI_CHAR, TRACKER_RANK, FILE_INFO_REQUEST_TAG, MPI_COMM_WORLD);
        MPI_Recv(&file_info, 1, MPI_FILE_INFO, TRACKER_RANK, FILE_INFO_RESPONSE_TAG, MPI_COMM_WORLD, &status);  
        file->nr_hashes = file_info.nr_hashes;
        for (int j = 0; j < file_info.nr_hashes; j++) {
            strncpy(file->hashes[j], file_info.hashes[j], HASH_SIZE);
        }
    }
    MPI_Send(NULL, 0, MPI_CHAR, TRACKER_RANK, STOP_FILE_INFO_REQUEST_TAG, MPI_COMM_WORLD);
    char resp;
    MPI_Recv(&resp, 1, MPI_CHAR, TRACKER_RANK, CLIENTS_READY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    MPI_Barrier(MPI_COMM_WORLD);
    // Debugging output before entering the while loop
    // printf("[Download Thread Rank %d] Computed values before downlod loop:\n", args->rank);
    // printf("  Number of Files to Download: %d\n", args->nr_downloads);
    // for (int i = 0; i < args->nr_downloads; i++) {
    //     printf("  File %d: %s\n", i, args->downloads[i].filename);
    //     printf("    Number of Hashes: %d\n", args->downloads[i].nr_hashes);
    //     for (int j = 0; j < args->downloads[i].nr_hashes; j++) {
    //         printf("    Hash %d: %s\n", j, args->downloads[i].hashes[j]);
    //     }
    // }
    // printf("  Total Tasks: %d\n", args->numtasks);
    // printf("  Initial Finished Files: %d\n", nr_finished_files);

    int *seeds_ids = malloc(args->numtasks * sizeof(int));
    int *peers_ids = malloc(args->numtasks * sizeof(int));
    for (int file_id = 0; file_id < args->nr_downloads; file_id++) {
        File_c *file = &args->downloads[file_id];
        
        while (file->nr_chunks < file->nr_hashes) {
            MPI_Send(file->filename, MAX_FILENAME, MPI_CHAR, TRACKER_RANK, SWARM_REQUEST_TAG, MPI_COMM_WORLD);
            MPI_Recv(swarm, args->numtasks, MPI_CHAR, TRACKER_RANK, SWARM_RESPONSE_TAG, MPI_COMM_WORLD, &status);
    
            memset(seeds_ids, 0, args->numtasks * sizeof(int));
            memset(peers_ids, 0, args->numtasks * sizeof(int));
            int nr_seeds = 0, nr_peers = 0;
            for (int j = 1; j < args->numtasks; j++) {
                if (swarm[j] == SEED) {
                    seeds_ids[nr_seeds++] = j;
                } else if (swarm[j] == PEER && j != args->rank) {
                    peers_ids[nr_peers++] = j;
                }
            }
            
            // Debugging output before downloading chunks
            // printf("[Download Thread Rank %d] State before downloading chunks of file %s :\n", args->rank, file->filename);
            // printf("  Number of Peers: %d\n", nr_peers);
            // printf("  Peer IDs: ");
            // for (int p = 0; p < nr_peers; p++) {
            //     printf("%d ", peers_ids[p]);
            // }
            // printf("\n");

            // printf("  Number of Seeds: %d\n", nr_seeds);
            // printf("  Seed IDs: ");
            // for (int s = 0; s < nr_seeds; s++) {
            //     printf("%d ", seeds_ids[s]);
            // }
            // printf("\n");

            // printf("  Swarm State: ");
            // for (int j = 1; j < args->numtasks; j++) {
            //     printf("%d ", swarm[j]);
            // }
            // printf("\n");
            
            int count_chunks = 0;
            Chunk_req chunk_req;
            memset(&chunk_req, 0, sizeof(Chunk_req));
            memcpy(chunk_req.filename, file->filename, MAX_FILENAME);
            // Try downloading from peers
            if (nr_peers > 0) {
                int peer = peers_ids[rand() % nr_peers];
                int chunks_from_peer = 1;   // if I start with a miss, I still pick another peer
                char resp;
                while (1) {
                    chunk_req.id = file->nr_chunks;
                    //printf("[%d] request chunk %d of file %s from peer %d\n", args->rank, chunk_req.id, chunk_req.filename, peer);
                    MPI_Send(&chunk_req, 1, MPI_CHUNK_REQ, peer, CHUNK_REQUEST_TAG, upload_comm);
                    MPI_Recv(&resp, 1, MPI_CHAR, peer, CHUNK_RESPONSE_TAG, MPI_COMM_WORLD, &status);
                  
                    if (resp == OK) {
                        //printf("[%d] received chunk %d of file %s from peer %d\n", args->rank, count_chunks, file->filename, peer);
                        pthread_mutex_lock(&file->mutex);
                        file->chunks[file->nr_chunks++] = 1;
                        pthread_mutex_unlock(&file->mutex);
                        count_chunks++;
                        chunks_from_peer++;
                        if (count_chunks == 10 || file->nr_hashes == file->nr_chunks) {
                            MPI_Send(NULL, 0, MPI_CHUNK_REQ, peer, CLOSE_CONNECTION_TAG, upload_comm);
                            break;
                        }
                    } else if (chunks_from_peer != 0) {
                        chunks_from_peer = 0;
                        //printf("[%d] change peer\n", args->rank);
                        peer = peers_ids[rand() % nr_peers];
                    } else {
                        break;
                    }
                }
            }
            if (count_chunks == 10 || file->nr_hashes == file->nr_chunks) {
                continue;
            }
            
            // Download from a seeder
            if (nr_seeds > 1) {
                shuffle(seeds_ids, nr_seeds);
            }
            // pick the least loaded seeder
            int seed = seeds_ids[0];
            int min_connections = INT_MAX, nr_connections;
            for (int i = 0; i < MIN(nr_seeds, 3) && min_connections > 0; i++) {
                MPI_Send(NULL, 0, MPI_CHUNK_REQ, seeds_ids[i], WORKLOAD_REQUEST_TAG, upload_comm);
                MPI_Recv(&nr_connections, 1, MPI_INT, seeds_ids[i], WORKLOAD_RESPONSE_TAG, MPI_COMM_WORLD, &status);
                //printf("[%d] seed %d has %d connections\n",args->rank, seeds_ids[i], nr_connections);
                if (nr_connections < min_connections) {
                    seed = seeds_ids[i];
                    min_connections = nr_connections;
                }
            }
            //printf("[%d] choose seed %d\n",args->rank, seed);
            while (count_chunks < 10 && file->nr_hashes != file->nr_chunks) { 
                chunk_req.id = file->nr_chunks;
                char resp;
                //printf("[%d] request chunk %d of file %s from seed %d\n", args->rank, chunk_req.id, chunk_req.filename, seed);
                MPI_Send(&chunk_req, 1, MPI_CHUNK_REQ, seed, CHUNK_REQUEST_TAG, upload_comm);
                MPI_Recv(&resp, 1, MPI_CHAR, seed, CHUNK_RESPONSE_TAG, MPI_COMM_WORLD, &status);
                if (resp == OK) {
                    //printf("[%d] received chunk %d of file %s\n", args->rank, file->nr_chunks, file->filename);
                    pthread_mutex_lock(&file->mutex);
                    file->chunks[file->nr_chunks++] = 1;
                    pthread_mutex_unlock(&file->mutex);
                    count_chunks++;
                }
            }
            MPI_Send(NULL, 0, MPI_CHUNK_REQ, seed, CLOSE_CONNECTION_TAG, upload_comm);
        }
        printf("[%d] I finished downloading file %s\n", args->rank, file->filename);
        MPI_Send(file->filename, MAX_FILENAME, MPI_CHAR, TRACKER_RANK, FINISHED_FILE_TAG, MPI_COMM_WORLD);
        write_output_file(args->rank, file);
    }
    printf("[%d] I finished downloading all files\n", args->rank);
    MPI_Send(msg, MAX_FILENAME, MPI_CHAR, TRACKER_RANK, FINISHED_ALL_FILES_TAG, MPI_COMM_WORLD);

    //free(swarm);
    free(msg);
    free(seeds_ids);
    free(peers_ids);
    return NULL;
}

void *upload_thread_func(void *arg)
{   
    Upload_args *args = (Upload_args *)arg;
    char resp;
    MPI_Status status;
    Chunk_req chunk_req;
    int nr_connections = 0;
    char *connections = calloc(args->numtasks, sizeof(char));
    for (int i = 0; i < args->numtasks; i++) {
        connections[i] = CLOSED;
    }
    while (1) {
        MPI_Recv(&chunk_req, 1, MPI_CHUNK_REQ, MPI_ANY_SOURCE, MPI_ANY_TAG, upload_comm, &status);

        if (status.MPI_TAG == CHUNK_REQUEST_TAG) {
            //printf("[%d] received request for chunk %d in %s from client %d\n", args->rank, chunk_req.id, chunk_req.filename ,status.MPI_SOURCE);
            resp = 0;
            for (int i = 0; i < args->nr_uploads; i++) {
                if (!strncmp(args->uploads[i].filename, chunk_req.filename, MAX_FILENAME)) {
                    resp = OK;
                    if (connections[status.MPI_SOURCE] == CLOSED) {
                        connections[status.MPI_SOURCE] = OPEN;
                        nr_connections++;
                    }
                    //printf("[%d] send chunk %d in %s to client %d\n", args->rank, chunk_req.id, chunk_req.filename, status.MPI_SOURCE);
                    MPI_Send(&resp, 1, MPI_CHAR, status.MPI_SOURCE, CHUNK_RESPONSE_TAG, MPI_COMM_WORLD);
                    break;
                }
            }
            if (resp != 0) {
                continue;
            }
            for (int i = 0; i < args->nr_downloads; i++) {
                if (!strncmp(args->downloads[i].filename, chunk_req.filename, MAX_FILENAME)) {
                    pthread_mutex_lock(&args->downloads[i].mutex);
                    if (args->downloads[i].nr_chunks < chunk_req.id) {
                        resp = NOT_FOUND;
                        if (connections[status.MPI_SOURCE] == OPEN) {
                            connections[status.MPI_SOURCE] = CLOSED;
                            nr_connections--;
                        }
                    } else {
                        resp = OK;
                        if (connections[status.MPI_SOURCE] == CLOSED) {
                            connections[status.MPI_SOURCE] = OPEN;
                            nr_connections++;
                        }
                    }
                    pthread_mutex_unlock(&args->downloads[i].mutex);
                    if (resp == OK) {
                        //printf("[%d] send chunk %d in %s to client %d\n", args->rank, chunk_req.id, chunk_req.filename ,status.MPI_SOURCE);
                    } else {
                        //printf("[%d] chunk %d in %s for client %d not found\n", args->rank, chunk_req.id, chunk_req.filename ,status.MPI_SOURCE);
                    }
                    MPI_Send(&resp, 1, MPI_CHAR, status.MPI_SOURCE, CHUNK_RESPONSE_TAG, MPI_COMM_WORLD);
                    break;
                }
            }
        } else if (status.MPI_TAG == CLOSE_CONNECTION_TAG) {
            //printf("[%d] close connection with client %d\n", args->rank, status.MPI_SOURCE);
            connections[status.MPI_SOURCE] = CLOSED;
            nr_connections--;
        } 
        else if (status.MPI_TAG == WORKLOAD_REQUEST_TAG) {
            //printf("[%d] responde to client %d with workload = %d\n", status.MPI_SOURCE, nr_connections, args->rank);
            MPI_Send(&nr_connections, 1, MPI_INT, status.MPI_SOURCE, WORKLOAD_RESPONSE_TAG, MPI_COMM_WORLD);
        } 
        else if (status.MPI_SOURCE == TRACKER_RANK && status.MPI_TAG == FINISHED_ALL_CLIENTS_TAG) {
            //printf("[%d] all clients are closed\n", args->rank);
            break;
        }
        else {
            printf("[%d] received wrong tag %d from %d\n", args->rank, status.MPI_TAG, status.MPI_SOURCE);
        }
    }
    free(connections);
    return NULL;
}


void tracker(int numtasks, int rank) {
    File_info *files = calloc(MAX_FILES, sizeof(File_info));
    File_info *files_post = calloc(MAX_FILES, sizeof(File_info));
    int nr_files = 0;
    char **swarms;
    int *is_downloading = calloc(numtasks, sizeof(int));
    char *msg = calloc(MAX_FILENAME + 1, sizeof(char));
    int nr_unfinished_clients = 0;
    MPI_Status status;

    swarms = malloc(MAX_FILES * sizeof(char *));
    for (int i = 0; i < MAX_FILES; i++) {
        swarms[i] = calloc(numtasks, sizeof(char));
    }
    
    // Receive file metadata from all clients
    for (int i = 1; i < numtasks; i++) {
        MPI_Recv(files_post, MAX_FILES, MPI_FILE_INFO, i, FILES_INFO_POST_TAG, MPI_COMM_WORLD, &status);
        int count;
        MPI_Get_count(&status, MPI_FILE_INFO, &count);
        for (int j = 0; j < count; j++) {
            for (int k = 0; k < MAX_FILES; k++) {
                if (!strncmp(files[k].filename, files_post[j].filename, MAX_FILENAME)) {
                    swarms[k][status.MPI_SOURCE] = SEED;
                    break;
                } else if (strlen(files[k].filename) == 0) {
                    strncpy(files[k].filename, files_post[j].filename, MAX_FILENAME);
                    files[k].nr_hashes = files_post[j].nr_hashes;
                    for (int l = 0; l < files_post[j].nr_hashes; l++) {
                        strncpy(files[k].hashes[l], files_post[j].hashes[l], HASH_SIZE);
                    }
                    swarms[k][status.MPI_SOURCE] = SEED;
                    nr_files++;
                    break;
                }
            }
        }
    }
    // Send signal to all clients
    char ready = OK;
    for (int i = 1; i < numtasks; i++) {
        MPI_Send(&ready, 1, MPI_CHAR, i, CLIENTS_READY_TAG, MPI_COMM_WORLD);
    }
    MPI_Barrier(MPI_COMM_WORLD);

    // Print debug information before entering the do-while loop
    // printf("[Tracker Rank %d] Computed values before loop:\n", rank);
    // printf("  Number of Files: %d\n", nr_files);
    // for (int i = 0; i < nr_files; i++) {
    //     printf("  File %d: %s\n", i, files[i].filename);
    //     printf("    Number of Hashes: %d\n", files[i].nr_hashes);
    //     for (int j = 0; j < files[i].nr_hashes; j++) {
    //         //printf("    Hash %d: %s\n", j, files[i].hashes[j]);
    //     }
    //     printf("    Swarm Size: %d\n", swarms_size[i]);
    //     printf("    Swarm Members: ");
    //     for (int j = 0; j < numtasks; j++) {
    //         if (swarms[i][j] != 0) {
    //             printf("%d ", j);
    //         }
    //     }
    //     printf("\n");
    // }

    // Receive requests for file metadata from all clients(download threads)
    for (int i = 1; i < numtasks;) {
        MPI_Recv(msg, MAX_FILENAME, MPI_CHAR, i, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        if (status.MPI_TAG == FILE_INFO_REQUEST_TAG) {
            printf("[%d] client %d will download file %s\n", rank, status.MPI_SOURCE, msg);
            for (int i = 0; i < nr_files; i++) {
                if (!strncmp(files[i].filename, msg, MAX_FILENAME)) {
                    if (is_downloading[status.MPI_SOURCE] == 0) {
                        is_downloading[status.MPI_SOURCE] = 1;
                        
                        nr_unfinished_clients++;
                    }
                    MPI_Send(&files[i], 1, MPI_FILE_INFO, status.MPI_SOURCE, FILE_INFO_RESPONSE_TAG, MPI_COMM_WORLD);
                    break;
                }
            }
        } else if (status.MPI_TAG == STOP_FILE_INFO_REQUEST_TAG) {
            i++;
        }
    }

    // Send signal to all clients
    for (int i = 1; i < numtasks; i++) {
        MPI_Send(&ready, 1, MPI_CHAR, i, CLIENTS_READY_TAG, MPI_COMM_WORLD);
    }
    MPI_Barrier(MPI_COMM_WORLD);

    do {
        MPI_Recv(msg, MAX_FILENAME, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        if (status.MPI_TAG == SWARM_REQUEST_TAG) {
            for (int i = 0; i < nr_files; i++) {
                if (!strncmp(files[i].filename, msg, MAX_FILENAME)) {
                    if (swarms[i][status.MPI_SOURCE] != PEER) {
                        swarms[i][status.MPI_SOURCE] = PEER;
                    }
                    MPI_Send(swarms[i], numtasks, MPI_CHAR, status.MPI_SOURCE, SWARM_RESPONSE_TAG, MPI_COMM_WORLD);
                    break;
                }
            }
        } else if (status.MPI_TAG == FINISHED_FILE_TAG) {
            printf("[%d] client %d finished downloading file %s\n", rank, status.MPI_SOURCE, msg);
            for (int i = 0; i < nr_files; i++) {
                if (!strncmp(files[i].filename, msg, MAX_FILENAME)) {
                    swarms[i][status.MPI_SOURCE] = SEED;
                }
            }
        } else if (status.MPI_TAG == FINISHED_ALL_FILES_TAG) {
            printf("[%d] client %d finished downloading all files\n", rank, status.MPI_SOURCE);
            if (is_downloading[status.MPI_SOURCE] == 1) {
                is_downloading[status.MPI_SOURCE] = 0;
                nr_unfinished_clients--;
            }
        } else {
            printf("[%d] received wrong tag %d\n", rank, status.MPI_TAG);
        }
        
    } while (nr_unfinished_clients > 0);

    printf("[%d] all clients finished downloading\n", rank);
    for (int i = 1; i < numtasks; i++) {
        MPI_Send(NULL, 0, MPI_CHUNK_REQ, i, FINISHED_ALL_CLIENTS_TAG, upload_comm);
    }
    //printf("[%d] tracker stop\n", rank);
    free(files);
    free(files_post);
    free(is_downloading);
    free(msg);
    for (int i = 0; i < MAX_FILES; i++) {
        free(swarms[i]);
    }
    free(swarms); 
}

void peer(int numtasks, int rank) {
    pthread_t download_thread;
    pthread_t upload_thread;
    void *status;
    int r;
    
    // Read source file
    char source_filename[MAX_FILENAME + 1];
    snprintf(source_filename, sizeof(source_filename), "in%d.txt", rank);
    FILE *source_file = fopen(source_filename, "r");

    int nr_uploads;
    fscanf(source_file, "%d\n", &nr_uploads);
    File_c *uploads = calloc(nr_uploads, sizeof(File_c));
    
    char hash[HASH_SIZE + 2];
    for (int i = 0; i < nr_uploads; i++) {
        fscanf(source_file, "%s %d\n", uploads[i].filename, &uploads[i].nr_hashes);
        uploads[i].nr_chunks = uploads[i].nr_hashes;
        for (int j = 0; j < uploads[i].nr_hashes; j++) {
            fgets(hash, sizeof(hash), source_file);

            // Remove trailing newline character
            size_t len = strlen(hash);
            if (len > 0 && hash[len - 1] == '\n') {
                hash[len - 1] = '\0';
                len--;
            }
            memcpy(uploads[i].hashes[j], hash, HASH_SIZE);
            uploads[i].chunks[j] = 1;
        }
    }

    int nr_downloads;
    fscanf(source_file, "%d\n", &nr_downloads);
    File_c* downloads = calloc(nr_downloads, sizeof(File_c));
    for (int i = 0; i < nr_downloads; i++) {
        fscanf(source_file, "%s\n", downloads[i].filename);
        pthread_mutex_init(&downloads[i].mutex, NULL);
    }
    fclose(source_file);

    // Send file metadata of uploaded files to the tracker 
    File_info* files_info = calloc(nr_uploads, sizeof(File_info));
    for (int i = 0; i < nr_uploads; i++) {
        strncpy(files_info[i].filename, uploads[i].filename, MAX_FILENAME);
        files_info[i].nr_hashes = uploads[i].nr_hashes;
        for (int j = 0; j < uploads[i].nr_hashes; j++) {
            strncpy(files_info[i].hashes[j], uploads[i].hashes[j], HASH_SIZE);
        }
    }
    MPI_Send(files_info, nr_uploads, MPI_FILE_INFO, TRACKER_RANK, FILES_INFO_POST_TAG, MPI_COMM_WORLD);
    char resp;
    // Wait for the tracker to signal that all clients submitted their files metadata
    MPI_Recv(&resp, 1, MPI_CHAR, TRACKER_RANK, CLIENTS_READY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    MPI_Barrier(MPI_COMM_WORLD);

    // Debugging before creating threads
    // printf("[Peer Rank %d] Computed values before creating threads:\n", rank);
    // printf("  Number of Uploads: %d\n", nr_uploads);
    // for (int i = 0; i < nr_uploads; i++) {
    //     printf("  Upload File %d: %s\n", i, uploads[i].filename);
    //     printf("    Number of Hashes: %d\n", uploads[i].nr_hashes);
    //     for (int j = 0; j < uploads[i].nr_hashes; j++) {
    //         printf("    Hash %d: %s\n", j, uploads[i].hashes[j]);
    //     }
    // }
    // printf("  Number of Downloads: %d\n", nr_downloads);
    // for (int i = 0; i < nr_downloads; i++) {
    //     printf("  Download File %d: %s\n", i, downloads[i].filename);
    // }

    Download_args *download_args = malloc(sizeof(Download_args));
    download_args->numtasks = numtasks;
    download_args->rank = rank;
    download_args->nr_downloads = nr_downloads;
    download_args->downloads = downloads;

    Upload_args *upload_args = malloc(sizeof(Upload_args));
    upload_args->numtasks = numtasks;
    upload_args->rank = rank;
    upload_args->nr_uploads = nr_uploads;
    upload_args->uploads = uploads;
    upload_args->nr_downloads = nr_downloads;
    upload_args->downloads = downloads;

    r = pthread_create(&download_thread, NULL, download_thread_func, (void *)download_args);
    if (r) {
        printf("Eroare la crearea thread-ului de download\n");
        exit(-1);
    }

    r = pthread_create(&upload_thread, NULL, upload_thread_func, (void *) upload_args);
    if (r) {
        printf("Eroare la crearea thread-ului de upload\n");
        exit(-1);
    }

    r = pthread_join(download_thread, &status);
    if (r) {
        printf("Eroare la asteptarea thread-ului de download\n");
        exit(-1);
    }

    r = pthread_join(upload_thread, &status);
    if (r) {
        printf("Eroare la asteptarea thread-ului de upload\n");
        exit(-1);
    }
    
    for (int i = 0; i < nr_downloads; i++) {
        pthread_mutex_destroy(&downloads[i].mutex);
    }
    free(files_info);
    free(uploads);
    free(downloads);
    free(upload_args);
    free(download_args);  
}

int main (int argc, char *argv[]) {
    int numtasks, rank;
 
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (provided < MPI_THREAD_MULTIPLE) {
        fprintf(stderr, "MPI nu are suport pentru multi-threading\n");
        exit(-1);
    }
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_dup(MPI_COMM_WORLD, &upload_comm);
    init_MPI_FILE_INFO();
    init_MPI_CHUNK_REQ();

    srand(time(NULL));

    if (rank == TRACKER_RANK) {
        tracker(numtasks, rank);
    } else {
        peer(numtasks, rank);
    }
    MPI_Type_free(&MPI_CHUNK_REQ);
    MPI_Type_free(&MPI_FILE_INFO);
    
    MPI_Finalize();
}
