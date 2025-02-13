#ifndef TEMA2_H
#define TEMA2_H

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))

#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 32
#define MAX_CHUNKS 100

#define PEER 1
#define SEED 2

#define OK 1
#define NOT_FOUND 2

#define OPEN 1
#define CLOSED 0

#define FILES_INFO_POST_TAG 0
#define CLIENTS_READY_TAG 1
#define FILE_INFO_REQUEST_TAG 2
#define FILE_INFO_RESPONSE_TAG 3
#define STOP_FILE_INFO_REQUEST_TAG 4
#define SWARM_REQUEST_TAG 5
#define SWARM_RESPONSE_TAG 6
#define CLOSE_CONNECTION_TAG 7
#define WORKLOAD_REQUEST_TAG 8
#define WORKLOAD_RESPONSE_TAG 9
#define CHUNK_REQUEST_TAG 10
#define CHUNK_RESPONSE_TAG 11
#define FINISHED_FILE_TAG 12
#define FINISHED_ALL_FILES_TAG 13
#define FINISHED_ALL_CLIENTS_TAG 14

MPI_Datatype MPI_FILE_INFO;
MPI_Datatype MPI_CHUNK_REQ;
MPI_Comm upload_comm;

typedef struct file_c {
    char filename[MAX_FILENAME + 1];
    int nr_hashes;
    char hashes[MAX_CHUNKS][HASH_SIZE + 1];
    int nr_chunks;
    char chunks[MAX_CHUNKS];
    pthread_mutex_t mutex;
} File_c;

typedef struct file_info {
    char filename[MAX_FILENAME + 1];
    int nr_hashes;
    char hashes[MAX_CHUNKS][HASH_SIZE + 1];
} File_info;

typedef struct chunk_req {
    char filename[MAX_FILENAME + 1];
    int id;
} Chunk_req;

typedef struct {
    int numtasks;
    int rank;
    int nr_downloads;
    File_c *downloads;
} Download_args;

typedef struct {
    int numtasks;
    int rank;
    int nr_uploads;
    File_c *uploads;
    int nr_downloads;
    File_c *downloads;
} Upload_args;

void init_MPI_FILE_INFO() {
    MPI_Datatype oldtypes[3];
    int blockcounts[3];
    MPI_Aint offsets[3];

    // filename
    offsets[0] = offsetof(File_info, filename);
    oldtypes[0] = MPI_CHAR;
    blockcounts[0] = MAX_FILENAME + 1;
 
    // nr_hashes
    offsets[1] = offsetof(File_info, nr_hashes);
    oldtypes[1] = MPI_INT;
    blockcounts[1] = 1;
 
    // hasher
    offsets[2] = offsetof(File_info, hashes);
    oldtypes[2] = MPI_CHAR;
    blockcounts[2] = MAX_CHUNKS * (HASH_SIZE + 1);
 
    // se defineste tipul nou si se comite
    MPI_Type_create_struct(3, blockcounts, offsets, oldtypes, &MPI_FILE_INFO);
    MPI_Type_commit(&MPI_FILE_INFO);
}

void init_MPI_CHUNK_REQ() {
    MPI_Datatype oldtypes[2];
    int blockcounts[2];
    MPI_Aint offsets[2];

    // filename
    offsets[0] = offsetof(Chunk_req, filename);
    oldtypes[0] = MPI_CHAR;
    blockcounts[0] = MAX_FILENAME + 1;
 
    // id
    offsets[1] = offsetof(Chunk_req, id);
    oldtypes[1] = MPI_INT;
    blockcounts[1] = 1;
 
    // se defineste tipul nou si se comite
    MPI_Type_create_struct(2, blockcounts, offsets, oldtypes, &MPI_CHUNK_REQ);
    MPI_Type_commit(&MPI_CHUNK_REQ);
}
#endif