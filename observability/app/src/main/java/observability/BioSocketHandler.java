package observability;

import io.micrometer.core.instrument.Tags;
import org.apache.log4j.Logger;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.net.Socket;
import java.util.concurrent.atomic.AtomicLong;

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
			String msg;

			AtomicLong curProdCount = new AtomicLong(0);
			AtomicLong curConsCount = new AtomicLong(0);
			AtomicLong curQueueSize = new AtomicLong(0);
			MetricExporter.getInstance().getRegistry().gauge("cur_prod_count", Tags.of("name", "cuong"), curProdCount, AtomicLong::get);
			MetricExporter.getInstance().getRegistry().gauge("cur_cons_count", Tags.of("name", "cuong"), curConsCount, AtomicLong::get);
			MetricExporter.getInstance().getRegistry().gauge("cur_queue_size", Tags.of("name", "cuong"), curQueueSize, AtomicLong::get);

			while(true) {
				msg = in.readLine();
				if (!msg.isEmpty()) {
					LOGGER.info(msg);

					String[] metrics = msg.split(":");
					curProdCount.set(Long.valueOf(metrics[0]));
					curConsCount.set(Long.valueOf(metrics[1]));
					curQueueSize.set(Long.valueOf(metrics[2]));

					MetricExporter.getInstance().getRegistry().counter("tmp_count", "name", "cuong").increment();
				}
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
