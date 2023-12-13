package observability;

import org.apache.log4j.Logger;

import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.SocketAddress;

public class BioSocketServer {
	private static final Logger LOGGER = Logger.getLogger(BioSocketServer.class);
	public void run() {
		ServerSocket serverSocket = null;
		Socket socket = null;
		try {
			//socket address
			SocketAddress socketAddress = new InetSocketAddress("127.0.0.1", 6873);
			// Create ServerScoket Server
			serverSocket = new ServerSocket();
			// Binding address
			serverSocket.bind(socketAddress);
			// Loop Blocking Waiting for Client Connection
			while (true) {
				socket = serverSocket.accept();
				//Create a new thread to execute the client's request and reply to the response
				Thread thread = new Thread(new BioSocketHandler(socket));
				thread.start();
			}

		} catch (IOException e) {
			e.printStackTrace();
		} finally {
			if (serverSocket != null) {
				try {
					serverSocket.close();
				} catch (IOException e) {
					e.printStackTrace();
				}
			}
			serverSocket = null;
		}
	}
}
