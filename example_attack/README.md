# Example Attack

This folder contains a small Go program which can be used to verify whether a webpage is vulnerable to the attack.

# Usage

To use the tool you provide the URL of the service and the start of the query string which an be used as an oracle. For example, the command:
``
./attack "https://compression.website/unsafe/my_token?TK"
``
will extract the CSRF of our sample website.