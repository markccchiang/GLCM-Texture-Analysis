import type { ReactNode } from 'react';

export function PanelSection({ title, actions, footer, children, bodyClassName }: { title: string; actions?: ReactNode; footer?: ReactNode; children: ReactNode; bodyClassName?: string }) {
  return (
    <section className="panel-section" aria-label={title}>
      <header className="panel-header">
        <span>{title}</span>
        {actions}
      </header>
      <div className={bodyClassName ?? 'panel-body'}>{children}</div>
      {footer && <footer className="panel-footer">{footer}</footer>}
    </section>
  );
}
