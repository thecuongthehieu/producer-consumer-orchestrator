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
			int tmp_count = 0;

			AtomicLong curReadCount = new AtomicLong(0);
			AtomicLong curWriteCount = new AtomicLong(0);
			AtomicLong curQueueSize = new AtomicLong(0);
			MetricExporter.getInstance().getRegistry().gauge("cur_read_count", Tags.of("name", "cuong"), curReadCount, AtomicLong::get);
			MetricExporter.getInstance().getRegistry().gauge("cur_write_count", Tags.of("name", "cuong"), curWriteCount, AtomicLong::get);
			MetricExporter.getInstance().getRegistry().gauge("cur_queue_size", Tags.of("name", "cuong"), curQueueSize, AtomicLong::get);

			while(true) {
				msg = in.readLine();
				if (!msg.isEmpty()) {
					tmp_count += 1;
					LOGGER.info(msg);

					String[] metrics = msg.split(":");
					curReadCount.set(Long.valueOf(metrics[0]));
					curWriteCount.set(Long.valueOf(metrics[1]));
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
