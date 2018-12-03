#include <iostream>
#include <vector>
#include <fstream>
#include <cstring>

#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <strings.h>

using namespace std;

int main(int argc, char * argv[])
{

    //Test to ensure the user has provided one argument as required.  If not, default the server hostname.  If so, take
	//the server hostname from the user.

	char defaultHostname[] = "home.luwsnet.com";
	char hostname[]= "";

    if(argc != 2)
    {
		cout << "Usage: sendCode [hostname]" << endl;
		cout << "Example: sendCode localhost" << endl;
		cout << endl << "The hostname is optional and if none is picked will default to Matt's home box" << endl;

		strcpy(hostname, defaultHostname);
    }
    else
    {
    	strcpy(hostname, argv[1]);
    }

	/*This part is mandatory to setup connections
	 *
	 */

	//Setup variables to establish a connection and send a JPG to the server
	int socketDescriptor1;
	struct sockaddr_in server;
	struct hostent *hp;
	int cnct;

	//Initiate a socket on the server
	socketDescriptor1 = socket(AF_INET, SOCK_STREAM, 0);
	if (socketDescriptor1 < 0) {
		//If the socket cannot be established throw an error and exit
		cout << "Socket cannot be established.  Exiting." << endl;
		exit(1);
	}
	cout << "Socket established successfully!" << endl;

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
		cout << "Unable to connect on the created socket.  Exiting." << endl;
		exit(1);
	}
	cout << "\tConnection established!" << endl;

	char buf[3000] = { ' ' };

	/* This is where you would implement your jpg capture from camera.  Currently using a file example as a placeholder
	 * Refer to the server side for a vector<char> example where jpg data was held in memory.  Can you do something
	 * similar on Android?
	 */

	int from;
	from = open("input.jpg", O_RDONLY);
	if (from < 0)
	{
		cout << "\tUnable to open the specified file" << endl;
		return 0;
	}


	//Get size of the file to be sent by seeking to the end and the reset the descriptor back
	int outgoingFilesize = lseek(from, 0, SEEK_END);
	lseek(from, 0, 0);

	int outgoingFilesizeConverted = htonl(outgoingFilesize);

	/* Need all the rest of the lines to keep network handshakes rolling.  At the end of this block
	 * there is a vector of jpg data that's come back in.  You will need to manipulate that into a proper format to
	 * display on Android I'm sure
	 */

	int n, s;
	s = write(socketDescriptor1, &outgoingFilesizeConverted, sizeof(outgoingFilesizeConverted));
	//Send data from the file



	while (true)
	{
		n = read(from, buf, sizeof(buf));
		s = write(socketDescriptor1, buf, n);

		if (s < 0)
		{
			cout<<"\tError sending data to the outgoing connection" << endl;
			exit(1);
		}

		if(n==0)
		{
			break;
		}
	}

	//Get data back after conversion
	int incomingFilesize, incomingFileSizeConverted;
	n = recv(socketDescriptor1, &incomingFilesize, sizeof(incomingFilesize),0);

	incomingFileSizeConverted = ntohl(incomingFilesize);

	cout << "\t" << incomingFileSizeConverted << " bytes incoming." << endl;

	int bytesRead = 0;
	vector<unsigned char>incomingData;
	while(bytesRead < incomingFileSizeConverted)
	{
		n = recv(socketDescriptor1,buf,sizeof(buf),0);

		if(n < 0)
		{
			cout<<"\tError receiving data from the incoming connection" << endl;
			exit(1);
		}

		//Append this loops data to the buffer
		incomingData.insert(incomingData.end(), &buf[0], &buf[n]);
		bytesRead += sizeof(buf);
	}

	/* Just above... incomingData is a char vector which contains the full jpg response from the server
	 *
	 */

	cout << "\t" << incomingData.size() << " bytes in data element!" << endl;;

	//Uncomment this to output a test file to the file system along the way...verification of good data
	cout << "\tWriting the file to the filesystem as test.jpg" << endl;
	ofstream outie("test.jpg");
	outie.write((const char*)&incomingData[0], incomingData.size());

	close(socketDescriptor1);
	shutdown(socketDescriptor1, 0);

	return 0;
}
