# syntax=docker/dockerfile:1

# Node 24 is the current LTS line. This image includes npm, nvm, Git, native
# build tools, zsh, and a non-root user with passwordless sudo.
FROM mcr.microsoft.com/devcontainers/javascript-node:5-24-bookworm

RUN npm install --global bun@1.3.14

WORKDIR /workspaces/pydong
RUN chown node:node /workspaces/pydong
USER node

EXPOSE 4321
CMD ["bun", "run", "dev", "--", "--host", "0.0.0.0"]
