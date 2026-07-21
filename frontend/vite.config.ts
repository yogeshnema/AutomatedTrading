import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

const backendProxy = {
  '/trade-library': {
    target: 'http://127.0.0.1:8101',
    changeOrigin: true,
    rewrite: (path: string) => path.replace(/^\/trade-library/, ''),
  },
  '/pricing': {
    target: 'http://127.0.0.1:8080',
    changeOrigin: true,
    rewrite: (path: string) => path.replace(/^\/pricing/, ''),
  },
  '/risk': {
    target: 'http://127.0.0.1:8081',
    changeOrigin: true,
    rewrite: (path: string) => path.replace(/^\/risk/, ''),
  },
  '/market-data': {
    target: 'http://127.0.0.1:8201',
    changeOrigin: true,
    rewrite: (path: string) => path.replace(/^\/market-data/, ''),
  },
}

export default defineConfig({
  plugins: [react()],
  server: {
    port: 5173,
    strictPort: true,
    proxy: backendProxy,
  },
  preview: {
    host: '0.0.0.0',
    port: 5173,
    strictPort: true,
    proxy: backendProxy,
  },
})
