# cf-nocompress

The repository contains a proof of concept mitigation for compression oracle attacks as detailed [here](https://blog.cloudflare.com/a-solution-to-compression-oracles-on-the-web/). The repository is split into two folders. The first, cf-nocompress, contains an NGINX plugin that uses selective compression to mitigate such attacks. The second, example_attack, is a tool which can verify if a website is vulnerable to the attack.

The websites https://compression.website/ and https://compression.website/unsafe/ demonstrate this mitigation in action.