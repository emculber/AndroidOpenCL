#include <linux/in.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <strings.h>
#include <endian.h>
#include "common.hpp"

#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <vector>
#include <fstream>
#include <cstring>
#include <jni.h>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

//using namespace std;

extern "C" jlong sendImage(jlong inputImage)
{
    cv::Mat &mat = *(cv::Mat *) inputImage;

    //Test to ensure the user has provided one argument as required.  If not, default the server hostname.  If so, take
	//the server hostname from the user.

	char hostname[] = "home.luwsnet.com";

    //Setup variables to establish a connection and send a JPG to the server
    int socketDescriptor1;
    struct sockaddr_in server;
    struct hostent *hp;
    int cnct;

    //Initiate a socket on the server
    socketDescriptor1 = socket(AF_INET, SOCK_STREAM, 0);
    if (socketDescriptor1 < 0) {
        //If the socket cannot be established throw an error and exit
        LOGD("Socket cannot be established.  Exiting.");
        exit(1);
    }
    LOGD("Socket established successfully!");

    //Set connection parameters for an IPv4 connection on port 5050 and connect using any available interface
    server.sin_family = AF_INET;
    server.sin_port = htons(5050);
    server.sin_addr.s_addr = INADDR_ANY;

    //Resolve the hostname or IP address provided by the user and assign it to the destination address
    hp = gethostbyname(hostname);
    bcopy((char *) hp->h_addr,(char *)&server.sin_addr.s_addr,hp->h_length);

    //Try to connect on the established socket
    cnct = connect(socketDescriptor1, (struct sockaddr*) &server, sizeof(server));

    //If a negative value was returned, connection failed.  Need to quit.
    if (cnct < 0) {
        LOGD("Unable to connect on the created socket.  Exiting.");
        exit(1);
    }
    LOGD("\tConnection established!");

    std::vector<unsigned char> outgoingData;

    std::vector<int>params(2);
    params[0] = cv::IMWRITE_JPEG_QUALITY;
    params[1] = 100; //full quality
    cv::imencode(".jpg", mat, outgoingData, params);

    int outgoingDataSize = outgoingData.size();
    int outgoingDataSizeConverted = htonl(outgoingDataSize);
    LOGD("\tSending %d total bytes.", outgoingDataSize);
    write(socketDescriptor1, &outgoingDataSizeConverted, sizeof(outgoingDataSizeConverted));
    write(socketDescriptor1, &outgoingData[0], outgoingData.size());
    LOGD("\t...done, waiting for response.");

    int n, s;
    char buf[3000]={' '};

    //Get data back after conversion
    int incomingFilesize, incomingFileSizeConverted;
    n = recv(socketDescriptor1, &incomingFilesize, sizeof(incomingFilesize),0);

    incomingFileSizeConverted = ntohl(incomingFilesize);

    LOGD("\t %d bytes incoming.", incomingFileSizeConverted);

    int bytesRead = 0;
    std::vector<unsigned char>incomingData;
    while(bytesRead < incomingFileSizeConverted)
    {
        n = recv(socketDescriptor1,buf,sizeof(buf),0);

        if(n < 0)
        {
            LOGD("\tError receiving data from the incoming connection");
            exit(1);
        }

        //Append this loops data to the buffer
        incomingData.insert(incomingData.end(), &buf[0], &buf[n]);
        bytesRead += sizeof(buf);
    }

    // Just above... incomingData is a char vector which contains the full jpg response from the server

    LOGD("\t %d bytes in data element!", incomingData.size());

    cv::Mat *outputMat = new cv::Mat();
    cv::imdecode(incomingData, 1).copyTo(*outputMat);

    close(socketDescriptor1);

    return (jlong)outputMat;
}
