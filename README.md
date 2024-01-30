# Xil — A Nix command-line interface for humans

## This software is a heavy work-in-progress and is at best alpha quality

The vast majority of features are not yet implemented. Most exceptions are not caught and do not yet print helpful
diagnostics.

Nonetheless it may still be useful to some people. Notably, `xil print` has functionality not available through other
Nix interfaces, such as pretty printing, printing expressions with errors, and `--call-package`/`-C`.
For example, here is the output of `xil print --call-package --expr '{ pkgs }: pkgs.nixVersions'`
(or `xil print -CE '{ pkgs }: pkgs.nixVersions'` for short):

```nix
{
    recurseForDerivations = true;
    overrideDerivation = «lambda fdrv: …»;
    __unfix__ = «lambda self: …»;
    extend = «lambda f: …»;
    override = {
        __functor = «lambda self: …»;
        __functionArgs = {
            pkgs = false;
        };
    };
    stable = «derivation /nix/store/np7rapdwijl64csi8q5cdabygyz5l17b-nix-2.18.1.drv»;
    nix_2_16 = «derivation /nix/store/ysgx5rr63bvzggnjcb12lm6zilmp0z8n-nix-2.16.2.drv»;
    nix_2_15 = «derivation /nix/store/40mzby19bzj9z3sps1zr0wf12rdihr1w-nix-2.15.3.drv»;
    nix_2_19 = «derivation /nix/store/vkswl86xwjwhyrj9p0kwm9rj2xvpf68w-nix-2.19.2.drv»;
    unstable = «derivation /nix/store/vkswl86xwjwhyrj9p0kwm9rj2xvpf68w-nix-2.19.2.drv»;
    nix_2_3 = «derivation /nix/store/9dsnpy3l1zymnl5ajh4iy71isrzp62l6-nix-2.3.17.drv»;
    nix_2_4 = «throws: nixVersions.nix_2_4 has been removed»;
    nix_2_5 = «throws: nixVersions.nix_2_5 has been removed»;
    nix_2_6 = «throws: nixVersions.nix_2_6 has been removed»;
    nix_2_10 = «derivation /nix/store/qlq3zkl3xlqp27x8ryld0765v6684zq4-nix-2.10.3.drv»;
    nix_2_11 = «derivation /nix/store/83sjw73czs4yid5mj7vviywylvbrfrp1-nix-2.11.1.drv»;
    nix_2_12 = «derivation /nix/store/jbim2xppskn5brwcxnb6sc42bkijjskm-nix-2.12.1.drv»;
    nix_2_13 = «derivation /nix/store/djdg4wgdvq3yj65xg6afbi17nj2b9bkc-nix-2.13.6.drv»;
    nix_2_14 = «derivation /nix/store/kzcjaa5gnzya2yzy5l7rnkivllqdnhq5-nix-2.14.1.drv»;
    nix_2_17 = «derivation /nix/store/s59xr3glaljjdxlxawnhp8pwcc8ll5ci-nix-2.17.1.drv»;
    nix_2_18 = «derivation /nix/store/np7rapdwijl64csi8q5cdabygyz5l17b-nix-2.18.1.drv»;
    minimum = «derivation /nix/store/9dsnpy3l1zymnl5ajh4iy71isrzp62l6-nix-2.3.17.drv»;
    nix_2_7 = «throws: nixVersions.nix_2_7 has been removed»;
    nix_2_8 = «throws: nixVersions.nix_2_8 has been removed»;
    nix_2_9 = «throws: nixVersions.nix_2_9 has been removed»;
}
```
