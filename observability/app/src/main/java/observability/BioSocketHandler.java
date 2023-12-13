package observability;

import org.apache.log4j.Logger;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;
import java.net.Socket;

public class BioSocketHandler implements Runnable {
	private static final Logger LOGGER = Logger.getLogger(BioSocketHandler.class);
	private Socket socket;

	public BioSocketHandler(Socket socket) {
		this.socket = socket;
	}

	@Override
	public void run() {
		BufferedReader in = null;
		PrintWriter out = null;
		try {
			in = new BufferedReader(new InputStreamReader(socket.getInputStream()));
			out = new PrintWriter(new OutputStreamWriter(socket.getOutputStream()), true);
			String request;
			while(true) {
				request = in.readLine();
				//Receive client requests and respond
				LOGGER.info(request);
				out.println("Bio Server response data response");
			}
		} catch (IOException e) {
			LOGGER.error(e.toString());
		} finally {
			if (in != null) {
				try {
					in.close();
				} catch (IOException e) {
					LOGGER.error(e.toString());
				}
			}
			if (out != null) {
				try {
					out.close();
				} catch (Exception e) {
					LOGGER.error(e.toString());
				}
			}
			if (socket != null) {
				try {
					socket.close();
				} catch (IOException e) {
					LOGGER.error(e.toString());
				}
			}
			socket = null;
		}
	}
}
