import type { ReactNode, Ref } from 'react';

export function PanelSection({
  title,
  actions,
  footer,
  children,
  bodyClassName,
  bodyRef,
}: {
  title: string;
  actions?: ReactNode;
  footer?: ReactNode;
  children: ReactNode;
  bodyClassName?: string;
  /** The scrolling body, e.g. for a virtualized list */
  bodyRef?: Ref<HTMLDivElement>;
}) {
  return (
    <section className="panel-section" aria-label={title}>
      <header className="panel-header">
        <span>{title}</span>
        {actions}
      </header>
      <div className={bodyClassName ?? 'panel-body'} ref={bodyRef}>
        {children}
      </div>
      {footer && <footer className="panel-footer">{footer}</footer>}
    </section>
  );
}
