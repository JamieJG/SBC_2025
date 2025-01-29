from flask import Flask, send_file, send_from_directory, render_template_string, abort
import os

app = Flask(__name__)

# Template básico para mostrar archivos y carpetas
HTML_TEMPLATE = """
<!doctype html>
<html>
<head>
    <title>Explorador de Archivos</title>
</head>
<body>
    <h1>Contenido de: {{ path }}</h1>
    <ul>
        {% for folder in folders %}
            <li><a href="{{ folder }}">{{ folder }}</a></li>
        {% endfor %}
        {% for file in files %}
            <li>{{ file }}</li>
        {% endfor %}
    </ul>
</body>
</html>
"""

@app.route('/', defaults={'subpath': ''})
@app.route('/<path:subpath>')
def browse_directory(subpath):
    # Calcula la ruta absoluta del directorio o subdirectorio
    full_path = os.path.join(os.getcwd(), subpath)
    
    if not os.path.exists(full_path) or not os.path.isdir(full_path):
        return abort(404, "Directorio no encontrado")

    # Lista los elementos de la ruta actual
    items = os.listdir(full_path)
    folders = [item for item in items if os.path.isdir(os.path.join(full_path, item))]
    files = [item for item in items if os.path.isfile(os.path.join(full_path, item))]

    # Renderiza el template HTML con los archivos y carpetas
    return render_template_string(HTML_TEMPLATE, path=subpath or "/", folders=[os.path.join(subpath, f) for f in folders], files=files)

@app.route('/favicon.ico')
def favicon():
    return send_from_directory(os.path.join(app.root_path, 'static'), 'favicon.ico')

@app.route('/firmware')
def serve_firmware():
    files = os.listdir('.')
    firmware_files = [f for f in files if f.endswith('bin')]

    if firmware_files:
        latest_file = max(firmware_files, key=lambda f: os.path.getmtime(f))
        firmware_path = os.path.join('.', latest_file)
        return send_file(firmware_path, as_attachment=True)
    else:
        return "Firmware no encontrado", 404

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8080)
