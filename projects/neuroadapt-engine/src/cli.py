import argparse
import sys
import json
from src.engine import (
    Matrix,
    NeuralPolicyNetwork,
    ExperienceReplayBuffer,
    TabularQLearning,
    GridWorldEnvironment,
    RewardGuardrail,
    MonteCarloEvaluator,
    serialize_dqn_weights,
    deserialize_dqn_weights
)

def main():
    parser = argparse.ArgumentParser(description="NeuroAdapt Engine CLI (Zero-Dependency RL Runtime)")
    subparsers = parser.add_subparsers(dest="command", required=True)

    # Tabular training command
    parser_train_tab = subparsers.add_parser("train-tabular", help="Train tabular Q-learning agent")
    parser_train_tab.add_argument("--episodes", type=int, default=50, help="Number of training episodes")
    parser_train_tab.add_argument("--output", type=str, default="q_table.json", help="Output path for Q-table")

    # DQN training command
    parser_train_dqn = subparsers.add_parser("train-dqn", help="Train Deep Q-Network agent")
    parser_train_dqn.add_argument("--episodes", type=int, default=30, help="Number of training episodes")
    parser_train_dqn.add_argument("--output", type=str, default="dqn_weights.bin", help="Output path for DQN weights")

    # Evaluation command
    parser_eval = subparsers.add_parser("evaluate", help="Evaluate policy using Monte Carlo rollouts")
    parser_eval.add_argument("--weights", type=str, default="dqn_weights.bin", help="Path to DQN weights")
    parser_eval.add_argument("--episodes", type=int, default=10, help="Number of evaluation episodes")

    args = parser.parse_args()

    if args.command == "train-tabular":
        print(f"[NeuroAdapt] Starting Tabular Q-Learning for {args.episodes} episodes...")
        env = GridWorldEnvironment(grid_size=4)
        agent = TabularQLearning(actions=[0, 1, 2, 3], alpha=0.1, gamma=0.99, epsilon=1.0, epsilon_decay=0.95)
        guardrail = RewardGuardrail(min_reward=-100.0, max_reward=100.0)
        
        for ep in range(args.episodes):
            state = env.reset()
            done = False
            while not done:
                state_tuple = tuple(state)
                action = agent.select_action(state_tuple)
                next_state, reward, done, _ = env.step(action)
                guardrail.validate(reward)
                agent.update(state_tuple, action, reward, tuple(next_state), done)
                state = next_state

        # Save Q-table
        data = {str(k): list(v) for k, v in agent.q_table.items()}
        with open(args.output, "w") as f:
            json.dump(data, f)
        print(f"[NeuroAdapt] Tabular training complete. Q-table saved to {args.output}")

    elif args.command == "train-dqn":
        print(f"[NeuroAdapt] Starting DQN Training for {args.episodes} episodes...")
        env = GridWorldEnvironment(grid_size=4)
        net = NeuralPolicyNetwork(input_dim=4, hidden_dim=8, output_dim=4)
        target_net = NeuralPolicyNetwork(input_dim=4, hidden_dim=8, output_dim=4)
        target_net.copy_weights_from(net)
        buffer = ExperienceReplayBuffer(capacity=500)
        guardrail = RewardGuardrail()

        for ep in range(args.episodes):
            state = env.reset()
            done = False
            while not done:
                # Select action via epsilon-greedy
                if len(buffer) < 10 or (import_random := __import__('random')).random() < 0.1:
                    action = __import__('random').randint(0, 3)
                else:
                    q_vals = net.forward(state)
                    action = q_vals.index(max(q_vals))
                
                next_state, reward, done, _ = env.step(action)
                guardrail.validate(reward)
                buffer.push((state, action, reward, next_state, done))
                state = next_state

                if len(buffer) >= 16:
                    batch = buffer.sample(16)
                    net.train_step(batch, target_net, gamma=0.99, lr=0.01)

        serialize_dqn_weights(net, args.output)
        print(f"[NeuroAdapt] DQN training complete. Weights saved to {args.output}")

    elif args.command == "evaluate":
        print(f"[NeuroAdapt] Evaluating policy from {args.weights}...")
        net = NeuralPolicyNetwork(input_dim=4, hidden_dim=8, output_dim=4)
        try:
            deserialize_dqn_weights(net, args.weights)
        except Exception as e:
            print(f"[NeuroAdapt] Failed to load weights: {e}", file=sys.stderr)
            sys.exit(1)
            
        env = GridWorldEnvironment(grid_size=4)
        evaluator = MonteCarloEvaluator(env, num_rollouts=args.episodes, horizon=20)
        
        def policy_fn(state):
            q_vals = net.forward(state)
            return q_vals.index(max(q_vals))
            
        avg_return = evaluator.evaluate(policy_fn)
        print(f"[NeuroAdapt] Monte Carlo Evaluation Average Return: {avg_return:.4f}")

if __name__ == "__main__":
    main()
