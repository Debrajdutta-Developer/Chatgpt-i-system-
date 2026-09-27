package factory;

import java.nio.file.Path;
import java.nio.file.Paths;

public class Main {
    public static void main(String[] args) {
        if (args.length < 1) {
            System.out.println("Usage: java factory.Main <server|test> [port]");
            return;
        }

        String mode = args[0];
        if ("server".equalsIgnoreCase(mode)) {
            int port = args.length > 1 ? Integer.parseInt(args[1]) : 9092;
            Path dataDir = Paths.get("./omnistream-data");
            System.out.println("Starting OmniStream MQ Broker on port " + port + "...");
            Core.BrokerServer server = new Core.BrokerServer(port, dataDir);
            new Thread(server).start();
            System.out.println("OmniStream MQ Broker is running. Press Ctrl+C to exit.");
        } else if ("test".equalsIgnoreCase(mode)) {
            System.out.println("Running embedded validation suite...");
            try {
                CoreTest.main(new String[0]);
                IntegrationTest.main(new String[0]);
                System.out.println("All embedded validations completed successfully.");
            } catch (Exception e) {
                System.err.println("Validation failed: " + e.getMessage());
                e.printStackTrace();
                System.exit(1);
            }
        } else {
            System.out.println("Unknown mode: " + mode);
        }
    }
}
