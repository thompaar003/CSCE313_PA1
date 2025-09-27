/*
	Original author of the starter code
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 2/8/20
	
	Please include your Name, UIN, and the date below
	Name:Aaron Thompson
	UIN:234001220
	Date:9/25/2025
*/
#include "common.h"
#include "FIFORequestChannel.h"
#include <sys/wait.h>
#include <vector>

using namespace std;


int main (int argc, char *argv[]) {
	int opt;
	int p = 1;
	double t = 0.0;
	int e = 1;
	int buffercapacity = MAX_MESSAGE;
	bool new_channel_requested = false;
	bool is_file_transfer = false;
	string output_dir = "received/";

	string filename = "";
	while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1) {
		switch (opt) {
			case 'p':
				p = atoi (optarg);
				break;
			case 't':
				t = atof (optarg);
				break;
			case 'e':
				e = atoi (optarg);
				break;
			case 'f':
				filename = optarg;
				is_file_transfer = true;
				break;
			case 'm':
				buffercapacity = atoi (optarg);
				break;
			case 'c':
				new_channel_requested = true;
				break;
		}
	}
	
	pid_t pid = fork();

	if(pid==0) {
		
		char *server_args[] = {(char*)"./server",(char*)"-m", (char*)to_string(buffercapacity).c_str(), NULL};
       		execvp("./server", server_args);

		perror("Execvp failed");
		exit(1);
	}

	sleep(1);


	FIFORequestChannel* main_channel = new FIFORequestChannel("control", FIFORequestChannel::CLIENT_SIDE);
    	FIFORequestChannel* new_channel = NULL;

    	if (new_channel_requested) {
        	MESSAGE_TYPE m = NEWCHANNEL_MSG;
        	main_channel->cwrite(&m, sizeof(m));

        	char new_channel_name[30];
        	main_channel->cread(new_channel_name, 30);

        	new_channel = new FIFORequestChannel(new_channel_name, FIFORequestChannel::CLIENT_SIDE);
    	}
    
    	// Determine which channel to use for requests
    	FIFORequestChannel* current_channel;
    	if (new_channel_requested) {
        	current_channel = new_channel;
    	} else {
        	current_channel = main_channel;
    	}

	if (is_file_transfer) {
        	// FILE TRANSFER LOGIC (Task 4.3)
       	 	
		string output_filepath = output_dir + filename;

        	// Step 1: Request the file length
        	filemsg fm(0, 0);
        	int len = sizeof(filemsg) + (filename.size() + 1);
        	char* request_buf = new char[len];
        	memcpy(request_buf, &fm, sizeof(filemsg));
        	strcpy(request_buf + sizeof(filemsg), filename.c_str());
        	current_channel->cwrite(request_buf, len);

        	__int64_t file_size;
        	current_channel->cread((char*)&file_size, sizeof(__int64_t));
        	delete[] request_buf;

        	// Step 2: Transfer the file in chunks

        	ofstream outfile(output_filepath, ios::binary | ios::out);
        	if (!outfile) {
            		cerr << "Error: Could not create output file." << endl;
            		// Handle cleanup
        	}

        	__int64_t current_offset = 0;
        	int remaining_bytes = file_size;
        	char* buffer = new char[buffercapacity];

        	while (remaining_bytes > 0) {
            		int bytes_to_read = min(remaining_bytes, buffercapacity);

            		filemsg chunk_fm(current_offset, bytes_to_read);
            		len = sizeof(filemsg) + (filename.size() + 1);
            		request_buf = new char[len];
            		memcpy(request_buf, &chunk_fm, sizeof(filemsg));
            		strcpy(request_buf + sizeof(filemsg), filename.c_str());
            		current_channel->cwrite(request_buf, len);
            		delete[] request_buf;

            		current_channel->cread(buffer, bytes_to_read);
            		outfile.write(buffer, bytes_to_read);

            		current_offset += bytes_to_read;
            		remaining_bytes -= bytes_to_read;
        	}

        	outfile.close();
        	delete[] buffer;
        	cout << "File transfer complete. Received file saved to " << output_filepath << endl;

    	} else if (p > 0 && t == 0.0 && e == 1) {
        	// MULTIPLE DATA POINT REQUEST (Task 4.2)
		string out_filename = "x" + to_string(p) + ".csv";

		ofstream outfile(output_dir + out_filename);

       	if (!outfile) {
       		cerr << "Error: Could not open " << out_filename << " for writing." << endl;
       	}
				
    
   		for (int i = 0; i < 1000; ++i) {
           		double current_time = i * 0.004;

            		// Request for ECG 1
            		datamsg ecg1_req(p, current_time, 1);
            		current_channel->cwrite((char*) &ecg1_req, sizeof(datamsg));
            		double ecg1_val;
            		current_channel->cread(&ecg1_val, sizeof(double));

            		// Request for ECG 2
            		datamsg ecg2_req(p, current_time, 2);
            		current_channel->cwrite((char*) &ecg2_req, sizeof(datamsg));
            		double ecg2_val;
            		current_channel->cread(&ecg2_val, sizeof(double));

            		outfile << current_time << "," << ecg1_val << "," << ecg2_val << endl;
        		}
        		outfile.close();
        		cout << "Successfully saved the first 1000 data points to " << out_filename  << endl;
    	} else {
        	datamsg x(p, t, e);
        	current_channel->cwrite((char*) &x, sizeof(datamsg));
        	double reply;
        	current_channel->cread(&reply, sizeof(double));
        	cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << reply << endl;
    	}

    // Closing both channels
    	if (new_channel_requested) {
        	MESSAGE_TYPE m = QUIT_MSG;
        	new_channel->cwrite(&m, sizeof(m));
       		delete new_channel;
   	}
    
    	MESSAGE_TYPE m = QUIT_MSG;
    	main_channel->cwrite(&m, sizeof(m));
    	delete main_channel;

    	int status;
    	waitpid(pid, &status, 0);
    	return 0;
}
