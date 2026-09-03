import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

// Servidor de dados fica em processo separado (server/) -- ver README.md,
// secao "Por que dois processos em dev". O proxy abaixo e so pra dev; em
// producao o proprio Express serve os estaticos buildados aqui.
const BACKEND_PORT = 5175;

export default defineConfig({
  root: 'web',
  plugins: [react()],
  build: {
    // nunca 'dist/' -- esse nome ja e do SDK C++ publicado pelo Makefile raiz
    // (ver .gitignore raiz: 'dist' sem barra e "unanchored").
    outDir: 'dist-app',
    emptyOutDir: true,
  },
  server: {
    port: 5173,
    proxy: {
      '/api': {
        target: `http://localhost:${BACKEND_PORT}`,
        changeOrigin: true,
      },
    },
  },
});
