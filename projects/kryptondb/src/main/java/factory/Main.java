package factory;

import java.io.File;
import java.util.Map;

public class Main {
    public static void main(String[] args) {
        System.out.println("Starting KryptonDB Engine Demonstration...");
        try {
            File tmpDir = new File(System.getProperty("java.io.tmpdir"), "kryptondb_demo");
            Core.Database db = new Core.Database(tmpDir);
            
            long tx1 = db.beginTransaction();
            db.put(tx1, "user:1", "{\"name\":\"Alice\",\"status\":\"active\"}");
            db.put(tx1, "user:2", "{\"name\":\"Bob\",\"status\":\"inactive\"}");
            db.commit(tx1);
            System.out.println("Transaction 1 committed successfully.");

            long tx2 = db.beginTransaction();
            String val = db.get(tx2, "user:1");
            System.out.println("Retrieved user:1 -> " + val);
            db.commit(tx2);

            // B+Tree Index Demo
            Core.BPlusTreeIndex index = new Core.BPlusTreeIndex();
            index.insert("user:1", "page_1");
            index.insert("user:2", "page_2");
            System.out.println("B+Tree Index size: " + index.size());

            // Query Parser Demo
            Map<String, Object> doc = Core.QueryParser.parseDocument(val);
            boolean match = Core.QueryParser.evaluate(doc, "name", "Alice");
            System.out.println("Query evaluation result (name == Alice): " + match);

            db.close();
            System.out.println("KryptonDB demo completed successfully.");
        } catch (Exception e) {
            System.err.println("Error in KryptonDB Main: " + e.getMessage());
            e.printStackTrace();
        }
    }
}
