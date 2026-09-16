package factory;

import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.*;

public class Main {
    public static void main(String[] args) {
        System.out.println("========================================");
        System.out.println("  ColumnaDB Engine CLI & Verification   ");
        System.out.println("========================================");

        try {
            Path dbPath = Paths.get("./columna_data");
            Core.StorageEngine storage = new Core.StorageEngine(dbPath);

            List<Core.ColumnSchema> cols = Arrays.asList(
                new Core.ColumnSchema("id", Core.DataType.INT),
                new Core.ColumnSchema("val", Core.DataType.INT)
            );
            Core.TableSchema schema = new Core.TableSchema("metrics", cols);

            int[] sampleData = new int[5000];
            for (int i = 0; i < 5000; i++) {
                sampleData[i] = (i < 2500) ? 42 : 100;
            }

            byte[] encoded = Core.ColumnBlockCodec.encodeRLE(sampleData);
            storage.writeColumnSegment("metrics", "val", encoded);
            System.out.println("[Main] Wrote 5000 rows RLE compressed to storage segment.");

            byte[] readPayload = storage.readColumnSegment("metrics", "val");
            int[] decoded = Core.ColumnBlockCodec.decodeRLE(readPayload);
            System.out.println("[Main] Successfully read and decoded " + decoded.length + " rows. First value: " + decoded[0]);

            String sql = "SELECT id, val FROM metrics WHERE val = 42";
            Core.SimpleQueryParser.QueryAST ast = Core.SimpleQueryParser.parse(sql);
            System.out.println("[Main] Parsed SQL Table: " + ast.tableName + ", WhereCol: " + ast.whereColumn + ", Value: " + ast.whereValue);

            Core.TableStatistics leftStats = new Core.TableStatistics();
            leftStats.rowCount = 1000;
            Core.TableStatistics rightStats = new Core.TableStatistics();
            rightStats.rowCount = 50000;
            String optimalPlan = Core.CostBasedOptimizer.optimizeJoin(schema, schema, leftStats, rightStats, "id");
            System.out.println("[Main] CBO Optimal Join Plan Selected: " + optimalPlan);

            System.out.println("[Main] ColumnaDB execution complete.");
        } catch (Exception e) {
            System.err.println("[Main] Error: " + e.getMessage());
            e.printStackTrace();
        }
    }
}
