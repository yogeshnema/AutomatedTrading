import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

export default defineConfig({
  plugins: [react()],
  server: {
    port: 5173,
    strictPort: true,
    proxy: {
      '/market-data': {
        target: 'http://127.0.0.1:8201',
        changeOrigin: true,
        rewrite: path => path.replace(/^\/market-data/, ''),
      },
      '/risk': {
        target: 'http://127.0.0.1:8081',
        changeOrigin: true,
        rewrite: path => path.replace(/^\/risk/, ''),
      },
    },
  },
})
