package factory;

import java.io.*;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.List;
import factory.Core.AetherKV;
import factory.Core.Row;

public class Main {
    public static void main(String[] args) {
        String dirPath = "./data";
        int port = 8080;
        String query = null;

        for (int i = 0; i < args.length; i++) {
            if (args[i].equals("--dir") && i + 1 < args.length) {
                dirPath = args[++i];
            } else if (args[i].equals("--port") && i + 1 < args.length) {
                port = Integer.parseInt(args[++i]);
            } else if (args[i].equals("--query") && i + 1 < args.length) {
                query = args[++i];
            }
        }

        File dbDir = new File(dirPath);
        try (AetherKV db = new AetherKV(dbDir, 1024 * 1024, 16)) {
            db.start();
            System.out.println("AetherKV Engine started successfully in directory: " + dbDir.getAbsolutePath());

            if (query != null) {
                System.out.println("Executing query: " + query);
                try {
                    List<Row> results = db.executeQuery(query);
                    System.out.println("Results:");
                    for (Row r : results) {
                        System.out.println(r);
                    }
                } catch (Exception e) {
                    System.err.println("Error executing query: " + e.getMessage());
                }
                return;
            }

            System.out.println("Starting TCP Server on port " + port + "...");
            try (ServerSocket serverSocket = new ServerSocket(port)) {
                while (true) {
                    Socket clientSocket = serverSocket.accept();
                    Thread.ofVirtual().start(() -> handleClient(clientSocket, db));
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private static void handleClient(Socket socket, AetherKV db) {
        try (socket;
             BufferedReader reader = new BufferedReader(new InputStreamReader(socket.getInputStream()));
             PrintWriter writer = new PrintWriter(socket.getOutputStream(), true)) {

            writer.println("Welcome to AetherKV TCP Interface. Enter SQL queries or 'EXIT' to quit.");
            String line;
            while ((line = reader.readLine()) != null) {
                if (line.trim().equalsIgnoreCase("EXIT")) {
                    writer.println("Goodbye!");
                    break;
                }
                try {
                    List<Row> results = db.executeQuery(line);
                    writer.println("OK: " + results.size() + " rows returned.");
                    for (Row r : results) {
                        writer.println(r.toString());
                    }
                } catch (Exception e) {
                    writer.println("ERROR: " + e.getMessage());
                }
                writer.println("---EOF---");
            }
        } catch (IOException e) {
            System.err.println("Client connection error: " + e.getMessage());
        }
    }
}
