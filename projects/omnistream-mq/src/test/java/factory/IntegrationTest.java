package factory;

import java.io.IOException;
import java.net.InetSocketAddress;
import java.nio.ByteBuffer;
import java.nio.channels.SocketChannel;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;

public class IntegrationTest {
    private static int checkCount = 0;

    private static void verify(boolean condition, String message) {
        if (!condition) {
            throw new AssertionError("Integration check failed: " + message);
        }
        checkCount++;
        System.out.println("[CHECK PASS] " + message);
    }

    public static void main(String[] args) throws Exception {
        System.out.println("=== Starting IntegrationTest ===");

        Path tempDir = Files.createTempDirectory("omni-integration-");
        int port = 19092;
        Core.BrokerServer server = new Core.BrokerServer(port, tempDir);
        Thread serverThread = new Thread(server);
        serverThread.start();

        // Wait for server to bind
        Thread.sleep(300);

        // Connect TCP client
        SocketChannel client = SocketChannel.open();
        client.connect(new InetSocketAddress("localhost", port));
        verify(client.isConnected(), "TCP client successfully connected to broker server");

        // Produce Message via Wire Protocol
        String topic = "events";
        int partition = 0;
        byte[] payload = "event-payload-99".getBytes();

        // Payload: [TopicLen(4)][Topic][Partition(4)][DataLen(4)][Data]
        ByteBuffer producePayload = ByteBuffer.allocate(4 + topic.length() + 4 + 4 + payload.length);
        producePayload.putInt(topic.length());
        producePayload.put(topic.getBytes());
        producePayload.putInt(partition);
        producePayload.putInt(payload.length);
        producePayload.put(payload);

        Core.WireFrame produceFrame = new Core.WireFrame(Core.CMD_PRODUCE, 101, producePayload.array());
        client.write(ByteBuffer.wrap(produceFrame.serialize()));

        // Read response
        ByteBuffer respBuf = ByteBuffer.allocate(1024);
        int n = client.read(respBuf);
        verify(n > 0, "Client received response from broker after produce");
        respBuf.flip();
        Core.WireFrame response1 = Core.WireFrame.deserialize(respBuf);
        verify(response1 != null, "Produce response frame is valid");
        verify(response1.streamId == 101, "Produce response matches stream ID 101");
        verify(response1.command == Core.CMD_RESPONSE, "Produce response command is CMD_RESPONSE");

        long assignedOffset = ByteBuffer.wrap(response1.payload).getLong();
        verify(assignedOffset == 0, "Broker assigned offset 0 to first produced message");

        // Consume Message via Wire Protocol
        // Payload: [TopicLen(4)][Topic][Partition(4)][StartOffset(8)][MaxCount(4)]
        ByteBuffer consumePayload = ByteBuffer.allocate(4 + topic.length() + 4 + 8 + 4);
        consumePayload.putInt(topic.length());
        consumePayload.put(topic.getBytes());
        consumePayload.putInt(partition);
        consumePayload.putLong(0L);
        consumePayload.putInt(10);

        Core.WireFrame consumeFrame = new Core.WireFrame(Core.CMD_CONSUME, 102, consumePayload.array());
        client.write(ByteBuffer.wrap(consumeFrame.serialize()));

        ByteBuffer respBuf2 = ByteBuffer.allocate(1024);
        client.read(respBuf2);
        respBuf2.flip();
        Core.WireFrame response2 = Core.WireFrame.deserialize(respBuf2);
        verify(response2 != null, "Consume response frame is valid");
        verify(response2.streamId == 102, "Consume response matches stream ID 102");

        // Parse consumed messages from response payload
        ByteBuffer cb = ByteBuffer.wrap(response2.payload);
        int count = cb.getInt();
        verify(count == 1, "Consumer retrieved exactly 1 message from broker log");

        int mLen = cb.getInt();
        byte[] mBytes = new byte[mLen];
        cb.get(mBytes);
        Core.Message consumedMsg = Core.Message.deserialize(ByteBuffer.wrap(mBytes));
        verify(consumedMsg.offset == 0, "Consumed message has correct offset 0");
        verify(java.util.Arrays.equals(consumedMsg.data, payload), "Consumed message data matches original payload");

        // Graceful Client & Server Disconnection
        client.close();
        verify(!client.isOpen(), "TCP client socket closed successfully");

        server.stop();
        serverThread.join(1000);
        verify(!serverThread.isAlive(), "Broker server terminated cleanly");

        System.out.println("=== IntegrationTest Completed Successfully: " + checkCount + " checks verified ===");
    }
}
