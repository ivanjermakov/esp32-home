from alpine:latest

run apk add nodejs npm

copy server server
copy index.html /
copy package.json /
copy tsconfig.json /
copy vite.config.ts /

run npm install
run npm run build:fe

entrypoint npm run start

