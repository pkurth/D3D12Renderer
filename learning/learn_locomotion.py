import gym
import gym_loco
import numpy as np
import os
import torch
import math

from stable_baselines3 import PPO
from stable_baselines3 import DQN
from stable_baselines3.common.vec_env import SubprocVecEnv, DummyVecEnv, VecNormalize
from stable_baselines3.common.env_util import make_vec_env
from stable_baselines3.common.utils import set_random_seed
from stable_baselines3.common.evaluation import evaluate_policy
from stable_baselines3.common.callbacks import BaseCallback
from stable_baselines3.common.monitor import Monitor
from stable_baselines3.common.results_plotter import load_results, ts2xy, plot_results
from stable_baselines3.common.env_checker import check_env
from stable_baselines3.common.policies import ActorCriticPolicy

from vec_monitor import VecMonitor


def make_env(env_id, rank, seed=0):
    """
    Utility function for multiprocessed env.

    :param env_id: (str) the environment ID
    :param num_env: (int) the number of environments you wish to have in subprocesses
    :param seed: (int) the inital seed for RNG
    :param rank: (int) index of the subprocess
    """
    def _init():
        env = gym.make(env_id)
        env.seed(seed + rank)
        return env
    set_random_seed(seed)
    return _init


class SaveOnBestTrainingRewardCallback(BaseCallback):
    def __init__(self, check_freq: int, log_dir: str, verbose=1):
        super(SaveOnBestTrainingRewardCallback, self).__init__(verbose)
        self.check_freq = check_freq
        self.log_dir = log_dir
        self.save_path = os.path.join(log_dir, 'best_model')
        self.best_mean_reward = -np.inf

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


class CustomActorCriticPolicy(ActorCriticPolicy):
    def __init__(self, 
        observation_space,
        action_space,
        lr_schedule,
        net_arch = None,
        activation_fn = torch.nn.Tanh,
        *args,
        **kwargs,
    ):
        # Just pass arguments to super class.
        super(CustomActorCriticPolicy, self).__init__(
            observation_space,
            action_space,
            lr_schedule,
            net_arch,
            activation_fn,
            *args,
            **kwargs,
        )
        self.force_deterministic = False

    def forward(self, obs, deterministic = False) :
        return super().forward(obs, deterministic or self.force_deterministic)
 
if __name__ == '__main__':

    #env_id = "LunarLander-v2"
    env_id = "loco-v0"

    #check_env(gym.make(env_id), warn=True)

    log_dir = "tmp/"
    os.makedirs(log_dir, exist_ok=True)
    
    num_cpu = 16
    torch.set_num_threads(num_cpu)

    env = SubprocVecEnv([make_env(env_id, i) for i in range(num_cpu)])
    env = VecMonitor(env, log_dir)
    #env = VecNormalize(env, norm_obs=True, norm_reward=True, clip_obs=100.)
    
    callback = SaveOnBestTrainingRewardCallback(check_freq=1000, log_dir=log_dir)

    policy_kwargs = dict(activation_fn=torch.nn.Tanh,
                     net_arch=[dict(pi=[128, 128], vf=[128, 128])])
    
    start_from_pretrained = True

    if start_from_pretrained :
        print("Starting from pretrained.")
        model = PPO.load(os.path.join(log_dir, 'best_model.zip'), env=env, policy=CustomActorCriticPolicy)
    else :
        # https://stable-baselines3.readthedocs.io/en/master/modules/ppo.html
        model = PPO(
            CustomActorCriticPolicy, 
            env, 
            verbose=2, 
            tensorboard_log=log_dir, 
            policy_kwargs=policy_kwargs,
            clip_range=0.1,
            batch_size=128,
            n_epochs = 10,
            n_steps=2048, 
            learning_rate = 2.5e-5,
            device="auto"
        )
    
    model.learn(
        callback=callback,
        total_timesteps=1e7,
    )


    # Export.

    model = PPO.load(os.path.join(log_dir, 'best_model.zip'), env=env, policy=CustomActorCriticPolicy)

    network = model.policy

    #print(network)
    #print(network.state_dict())

    example = torch.Tensor(env.observation_space.sample()).unsqueeze(0)
    print("Example input", example)

    network = network.cpu()
    network.force_deterministic = True
    network = network.eval()

    print("Example output", network(example))

    traced_script_module = torch.jit.trace(network, example)
    traced_script_module.save(log_dir + "testexport.pt")


