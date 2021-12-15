import torch
import torch.nn as nn

class Navigator(nn.Module):
    def __init__(self):
        super(Navigator, self).__init__()
        self.lin = nn.Sequential(
            nn.Linear(3, 128),
            nn.SiLU(),
            nn.Linear(128, 128),
            nn.SiLU(),
            nn.Linear(128, 4)
        )

    def forward(self, x):
        return self.lin(x)
