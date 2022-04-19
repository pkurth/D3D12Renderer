import numpy as np
import os
import torch
import math
import util

from stable_baselines3 import PPO
from stable_baselines3 import DQN
from stable_baselines3.common.utils import set_random_seed
from stable_baselines3.common.evaluation import evaluate_policy
from stable_baselines3.common.callbacks import BaseCallback
from stable_baselines3.common.results_plotter import load_results, ts2xy, plot_results
from stable_baselines3.common.env_checker import check_env
from stable_baselines3.common.policies import ActorCriticPolicy



class SaveOnBestTrainingRewardCallback(BaseCallback):
    def __init__(self, check_freq: int, trace_freq: int, log_dir: str, verbose=1):
        super(SaveOnBestTrainingRewardCallback, self).__init__(verbose)
        self.check_freq = check_freq
        self.trace_freq = trace_freq
        self.log_dir = log_dir
        self.save_path = os.path.join(log_dir, 'best_model')
        self.best_mean_reward = -np.inf
        self.best_counter = 0

    def _init_callback(self) -> None:
        # Create folder if needed
        if self.save_path is not None:
            os.makedirs(self.save_path, exist_ok=True)

    def _on_step(self) -> bool:
        if self.n_calls % self.check_freq == 0:

          # Retrieve training reward
          x, y = ts2xy(load_results(self.log_dir), 'timesteps')
          if len(x) > 0:
              # Mean training reward over the last 100 episodes
              mean_reward = np.mean(y[-100:])
              if self.verbose > 0:
                print("Num timesteps: {}".format(self.num_timesteps))
                print("Best mean reward: {:.2f} - Last mean reward per episode: {:.2f}".format(self.best_mean_reward, mean_reward))

              # New best model, you could save the agent here
              if mean_reward > self.best_mean_reward:
                  self.best_mean_reward = mean_reward
                  # Example for saving best model
                  if self.verbose > 0:
                    print("Saving new best model to {}".format(self.save_path))
                  self.model.save(self.save_path)
                  self.best_counter += 1
                  
                  if self.best_counter % self.trace_freq == 0:
                      util.trace_model(self.model, self.model.get_env(), log_dir)


if __name__ == '__main__':

    log_dir = "tmp/"
    os.makedirs(log_dir, exist_ok=True)
    
    env = util.make_loco_env(log_dir)
    
    callback = SaveOnBestTrainingRewardCallback(
        check_freq=1000, 
        trace_freq=5, 
        log_dir=log_dir
    )

    policy_kwargs = dict(activation_fn=torch.nn.Tanh,
                     net_arch=[dict(pi=[128, 128], vf=[128, 128])])
    
    start_from_pretrained = False

    device = "cpu"

    if start_from_pretrained :
        print("Starting from pretrained.")
        model = PPO.load(os.path.join(log_dir, 'best_model.zip'), env=env, policy=ActorCriticPolicy, device=device)
    else :
        # https://stable-baselines3.readthedocs.io/en/master/modules/ppo.html
        model = PPO(
            ActorCriticPolicy, 
            env, 
            verbose=2, 
            #tensorboard_log=log_dir, 
            policy_kwargs=policy_kwargs,
            clip_range=0.1,
            batch_size=128,
            n_epochs = 10,
            n_steps=2048, 
            learning_rate = 2.5e-5,
            device=device
        )

        def init_weights(m):
            m.weight.data.uniform_(-0.01, 0.01)
            m.bias.data.fill_(0.0)

        #print(model.policy)
        model.policy.action_net.apply(init_weights)
    
    model.learn(
        callback=callback,
        total_timesteps=1e8,
    )


    # Export.

    model = PPO.load(os.path.join(log_dir, 'best_model.zip'), env=env, policy=ActorCriticPolicy, device=device)

    util.trace_model(model, env, log_dir)


