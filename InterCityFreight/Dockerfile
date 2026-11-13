FROM python:3.11-slim

# 1. Install C++ build tools (gcc, cmake, make)
RUN apt-get update && apt-get install -y --no-install-recommends \
    g++ \
    cmake \
    make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# 2. Copy project files
COPY . .

# 3. Build the C++ engine
# This compiles the code and puts the binary in algorithm_engine/freight_engine
RUN cmake -S algorithm_engine -B algorithm_engine/build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build algorithm_engine/build --parallel \
    && test -x algorithm_engine/freight_engine

# 4. Install Python dependencies + Gunicorn
RUN pip install --no-cache-dir -r requirements.txt gunicorn

# 5. Expose port (Render uses $PORT env var dynamically)
EXPOSE 10000

# 6. Start command
# --chdir backend ensures Python imports work correctly
CMD gunicorn --bind 0.0.0.0:${PORT:-10000} --chdir backend --workers 2 --timeout 120 app:app
