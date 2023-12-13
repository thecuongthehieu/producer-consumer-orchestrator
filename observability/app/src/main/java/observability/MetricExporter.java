package observability;

import com.sun.net.httpserver.HttpServer;
import io.micrometer.core.instrument.Tag;
import io.micrometer.prometheus.PrometheusConfig;
import io.micrometer.prometheus.PrometheusMeterRegistry;
import org.apache.log4j.Logger;

import java.io.IOException;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.util.List;

public class MetricExporter {
	private static final Logger LOGGER = Logger.getLogger(MetricExporter.class);

	private Thread serverThread;

	private PrometheusMeterRegistry prometheusRegistry;

	public boolean startMetricServer(int port) {
		if (serverThread != null) {
			// Dont know what we should do here actually. Need to understand about HttpServer more
			serverThread.interrupt();
			serverThread = null;
		}

		prometheusRegistry = new PrometheusMeterRegistry(PrometheusConfig.DEFAULT);
		try {
			HttpServer server = HttpServer.create(new InetSocketAddress(port), 0);
			server.createContext(
					"/prometheus",
					httpExchange -> {
						String response = prometheusRegistry.scrape();
						httpExchange.sendResponseHeaders(200, response.getBytes().length);
						try (OutputStream os = httpExchange.getResponseBody()) {
							os.write(response.getBytes());
						}
					});

			serverThread = new Thread((server::start));
			serverThread.start();
			return true;
		} catch (IOException e) {
			LOGGER.error(e.toString());
			return false;
		}
	}

	public void stopMetricServer() {
		serverThread.interrupt();
	}

	public void count(String metric, String... tags) {
		prometheusRegistry.counter(metric, tags).increment();
	}

	public void gauge(String metric, List<Tag> tags, long number) {
		prometheusRegistry.gauge(metric, tags, number);
	}

	public void dist(String metric, List<Tag> tags, long number) {
		var summary = prometheusRegistry.summary(metric, tags);
		summary.record(number);
	}
}
