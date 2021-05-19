import torch
import util
import os

from stable_baselines3 import PPO
from stable_baselines3.common.policies import ActorCriticPolicy

def write_network(network, file, variable_names) :

    i = 0
    for name, param in network.named_parameters():
        #print(name, param, param.size())
        var = variable_names[i]
        i += 1
        size = list(param.size())
        if len(size) == 2:
            file.write("static const float " + var + "[" + str(size[0]) + "][" + str(size[1]) + "] =\n{\n");
            for row in param.tolist() :
                file.write("\t{ ") # Row start.
                for val in row :
                    file.write(str(val))
                    file.write("f, ")
                file.write("},\n") # Row end.
            file.write("};\n\n") # 2D end.
        elif len(size) == 1:
            file.write("static const float " + var + "[" + str(size[0]) + "] =\n{\n\t");
            for val in param.tolist() :
                file.write(str(val))
                file.write("f, ")
            file.write("\n};\n\n")


def write_policy(policy, log_dir) :

    print(policy)

    with open(log_dir + "network.h", "w") as file :

        file.write("#pragma once\n\n")
        file.write("#define HIDDEN_LAYER_SIZE 128\n\n")

        policy_net = policy.mlp_extractor.policy_net
        write_network(policy_net, file, ["policyWeights1", "policyBias1", "policyWeights2", "policyBias2"])

        action_net = policy.action_net
        write_network(action_net, file, ["actionWeights", "actionBias"])


if __name__ == '__main__':
    
    log_dir = "tmp/"
    device = "cpu"

    env = util.make_loco_env(log_dir)

    model = PPO.load(os.path.join(log_dir, 'best_model.zip'), env=env, policy=ActorCriticPolicy, device=device)

    policy = model.policy
    write_policy(policy, log_dir)


