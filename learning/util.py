import gym
import loco_env
import torch

from stable_baselines3.common.vec_env import SubprocVecEnv, DummyVecEnv, VecNormalize
from stable_baselines3.common.env_util import make_vec_env
from stable_baselines3.common.utils import set_random_seed
from stable_baselines3.common.monitor import Monitor

from vec_monitor import VecMonitor


def make_env(env_type, rank, seed=0):
    """
    Utility function for multiprocessed env.

    :param env_type: (type) the environment type
    :param num_env: (int) the number of environments you wish to have in subprocesses
    :param seed: (int) the inital seed for RNG
    :param rank: (int) index of the subprocess
    """
    def _init():
        env = env_type()
        env.seed(seed + rank)
        return env
    set_random_seed(seed)
    return _init


def make_loco_env(log_dir) :
    env_type = loco_env.LocoEnv
    num_cpu = 16
    
    env = SubprocVecEnv([make_env(env_type, i) for i in range(num_cpu)])
    env = VecMonitor(env, log_dir)
    torch.set_num_threads(num_cpu)
    return env


class ExportPolicy(torch.nn.Module):
    def __init__(self, policy) :
        super(ExportPolicy, self).__init__()
        self.policy = policy

    def forward(self, obs) :
        # This is what the ActorCriticPolicy does internally to compute the actions.

        # policies.py -> forward -> _get_latent
        features = self.policy.extract_features(obs)

        # policies.py -> forward -> _get_latent -> mlp_extractor -> forward
        shared_latent = self.policy.mlp_extractor.shared_net(features)
        latent_pi = self.policy.mlp_extractor.policy_net(shared_latent)

        # policies.py -> forward -> distribution.get_actions(deterministic=true) -> distribution.mode (do not sample).
        actions = self.policy.action_net(latent_pi)
        return actions
    

def trace_model(model, env, log_dir) :
    
    path = log_dir + "testexport.pt"
    
    example = torch.Tensor(env.observation_space.sample()).unsqueeze(0)
    
    policy = model.policy
    policy = ExportPolicy(policy)
    
    #print("Example input", example)
    #print("Example output", policy(example))

    traced_script_module = torch.jit.trace(policy, example)
    traced_script_module.save(path)

    print("Model traced to", path)
