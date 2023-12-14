package observability;

import com.sun.net.httpserver.HttpServer;
import io.micrometer.prometheus.PrometheusConfig;
import io.micrometer.prometheus.PrometheusMeterRegistry;
import org.apache.log4j.Logger;

import java.io.IOException;
import java.io.OutputStream;
import java.net.InetSocketAddress;

public class MetricExporter {
	private static final Logger LOGGER = Logger.getLogger(MetricExporter.class);
	private static final int PORT = 9808;

	private static final MetricExporter INSTANCE = new MetricExporter();

	public static MetricExporter getInstance() {
		return INSTANCE;
	}

	private final PrometheusMeterRegistry prometheusRegistry;

	MetricExporter() {
		prometheusRegistry = new PrometheusMeterRegistry(PrometheusConfig.DEFAULT);
		try {
			HttpServer server = HttpServer.create(new InetSocketAddress(PORT), 0);
			server.createContext(
				"/prometheus",
				httpExchange -> {
					String response = prometheusRegistry.scrape();
					httpExchange.sendResponseHeaders(200, response.getBytes().length);
					try (OutputStream os = httpExchange.getResponseBody()) {
						os.write(response.getBytes());
					}
				});

			new Thread((server::start)).start();
		} catch (IOException e) {
			LOGGER.error(e.toString());
		}
	}

	public PrometheusMeterRegistry getRegistry() {
		return this.prometheusRegistry;
	}
}
