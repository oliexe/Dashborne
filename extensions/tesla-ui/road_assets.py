"""Original vector road artwork, rasterized once for the native image controls."""
from io import BytesIO
from PIL import Image
import resvg_py


def generate(output):
    def svg(content, width=600, height=360):
        return f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">{content}</svg>'

    def save(name, content, width=600, height=360):
        data = resvg_py.svg_to_bytes(svg_string=svg(content, width, height))
        (output / (name + '.png')).write_bytes(data)
        return Image.open(BytesIO(data)).convert('RGBA')

    save('road', '''
      <defs><linearGradient id="surface" x2="0" y2="1">
        <stop stop-color="#15181d" stop-opacity="0"/>
        <stop offset=".35" stop-color="#15181d" stop-opacity=".8"/>
        <stop offset="1" stop-color="#111317"/>
      </linearGradient></defs>
      <path d="M300 0 L0 360 H600 Z" fill="#0b0d10"/>
      <path d="M300 0 L90 360 H510 Z" fill="url(#surface)"/>
      <path d="M300 0 L90 360 M300 0 L510 360" fill="none" stroke="#4e535b" stroke-opacity=".22" stroke-width="1"/>
      <path d="M300 0 L-110 360 M300 0 L710 360" fill="none" stroke="#343a43" stroke-opacity=".28" stroke-width="1"/>
    ''')
    save('road-fog', '''
      <defs><linearGradient id="fog" x2="0" y2="1">
        <stop stop-color="black"/>
        <stop offset=".20" stop-color="black"/>
        <stop offset=".43" stop-color="black" stop-opacity="0"/>
        <stop offset=".74" stop-color="black" stop-opacity="0"/>
        <stop offset="1" stop-color="black"/>
      </linearGradient></defs><path d="M0 0 H600 V360 H0 Z" fill="url(#fog)"/>
    ''')
    left = save('road-dash-left', '<path d="M38 0 L2 64" stroke="#a9adb4" stroke-opacity=".65" stroke-width="2.4"/>', 40, 64)
    left.transpose(Image.Transpose.FLIP_LEFT_RIGHT).save(output / 'road-dash-right.png')
    for name, color in (('turn', '#57ba73'), ('blind', '#eaaa43'), ('alert', '#ef514e')):
        warning = '' if name == 'turn' else f'''
          <path d="M171 203 L183 224 H159 Z" fill="{color}" fill-opacity=".15" stroke="{color}" stroke-width="1.6" stroke-linejoin="round"/>
          <path d="M171 210 V216" stroke="{color}" stroke-width="1.8" stroke-linecap="round"/>
          <circle cx="171" cy="220" r="1" fill="{color}"/>
        '''
        left = save('road-left-' + name, f'''
          <defs><linearGradient id="zone" x2="0" y2="1">
            <stop stop-color="{color}" stop-opacity="0"/>
            <stop offset=".45" stop-color="{color}" stop-opacity=".20"/>
            <stop offset="1" stop-color="{color}" stop-opacity="0"/>
          </linearGradient><linearGradient id="edge" x2="0" y2="1">
            <stop stop-color="{color}" stop-opacity="0"/>
            <stop offset=".4" stop-color="{color}" stop-opacity=".9"/>
            <stop offset="1" stop-color="{color}" stop-opacity="0"/>
          </linearGradient></defs>
          <path d="M239 105 L131 290 H32 L196 105 Z" fill="url(#zone)"/>
          <path d="M239 105 L131 290" stroke="url(#edge)" stroke-width="3"/>
          {warning}
        ''')
        # Keep decoded warning textures small; they only occupy the side wedge.
        left = left.crop((30, 103, 242, 292))
        left.save(output / ('road-left-' + name + '.png'))
        left.transpose(Image.Transpose.FLIP_LEFT_RIGHT).save(output / ('road-right-' + name + '.png'))
